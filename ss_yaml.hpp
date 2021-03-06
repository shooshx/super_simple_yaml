#pragma once

#include <map>
#include <vector>
#include <string>
#include <memory>

namespace ss_yaml {
using namespace std;




#define CHECK(cond) do { if (!(cond)) throw std::runtime_error("failed CHECK(" #cond ")"); } while(false)
#define FAIL(text) do { throw std::runtime_error(text); } while(false)

extern "C" double my_strtod(const char *string, char **endPtr);

struct Str {
    Str(const char* s, int sz) : start(s), size(sz) {}
    explicit Str(const char* s) : start(s), size((int)strlen(s)) {}
    explicit Str(const string& s) : start(s.data()), size((int)s.size()) {}
    const char* end() { return start + size; } // one after the last character

    const char* start;
    int size;
};
bool operator==(const Str& a, const Str& b) {
    return a.size == b.size && memcmp(a.start, b.start, a.size) == 0;
}
template<typename T>
bool operator==(const Str& a, const T& b) {
    return operator==(a, Str(b));
}
bool operator<(const Str& a, const Str& b) {
    if (a.size != b.size)
        return a.size < b.size;
    return memcmp(a.start, b.start, a.size) < 0;
}


enum ENodeType {
    NODE_NONE = 0,
    NODE_MAP = 1,
    NODE_LIST,
    NODE_NUM_DBL,
    NODE_NUM_LONG,
    NODE_NUM_INT,
    NODE_STR,
};

struct Node {
    unsigned char type;  // ENodeType
};

struct DataNode : public Node {
    union {
        struct {
            int pos;
            int size;
        } str;
        double num_dbl;
        int64_t num_long;
        int num_int;
    };
};

struct MapNode : public Node {
    map<Str, Node*> v;
};
struct ListNode : public Node {
    vector<Node*> v;
};


class Yaml;

struct Accessor
{
    Node* node;
    Yaml* owner;

    Accessor(Node* _node, Yaml* _owner) : node(_node), owner(_owner) {}
    virtual ~Accessor() {}
    virtual Accessor operator[](int index) { return getOp(node->type).op_sq_int(this, index); }  //FAIL("operator[int] not implemented for this node"); }
    virtual Accessor operator[](const string& key) { return getOp(node->type).op_sq_str(this, key); } //FAIL("operator[str] not implemented for this node"); }
    virtual Accessor operator[](const char* key) { return getOp(node->type).op_sq_chp(this, key); } //FAIL("operator[char*] not implemented for this node"); }
    virtual Accessor nodeWith(const string& name, const string& key) { return getOp(node->type).nodeWith(this, name, key); } //FAIL("nodeWith not implemented for this node"); }
    virtual Accessor tryNodeWith(const string& name, const string& key) { return getOp(node->type).tryNodeWith(this, name, key); } // FAIL("tryNodeWith not implemented for this node");

    virtual string str() { return getOp(node->type).str(this); } // FAIL("str not implemented for this node"); }
    virtual double dbl() { return getOp(node->type).dbl(this); } // FAIL("flt not implemented for this node"); }
    virtual int len() { return getOp(node->type).len(this); } //FAIL("len not implemented for this node"); }

    bool isNull() const { return node == nullptr; }

    template<typename MatT, int sz>
    MatT mat()
    {
        CHECK(this->len() == sz);
        MatT ret;
        for (int i = 0; i < sz; ++i) {
            Node& line = (*this)[i];
            CHECK(line.len() == sz);
            for (int j = 0; j < sz; ++j)
                ret(i, j) = line[j].dbl();
        }
        return ret;
    }


    // ---------------
    static Accessor map_sq_str(Accessor* that, const string& key) {
        auto& v = ((MapNode*)that->node)->v;
        auto it = v.find(Str(key));
        CHECK(it != v.end());
        return Accessor(it->second, that->owner);
    }
    static Accessor map_sq_chp(Accessor* that, const char* key) {
        auto& v = ((MapNode*)that->node)->v;
        auto it = v.find(Str(key));
        CHECK(it != v.end());
        return Accessor(it->second, that->owner);
    }
    static int map_len(Accessor* that) {
        auto& v = ((MapNode*)that->node)->v;
        return (int)v.size();
    }
    
    // -------------
    static Accessor list_sq_int(Accessor* that, int index) {
        auto& v = ((ListNode*)that->node)->v;
        return Accessor(v[index], that->owner);
    }
    static int list_len(Accessor* that) {
        auto& v = ((ListNode*)that->node)->v;
        return (int)v.size();
    }
    static Accessor list_nodeWith(Accessor* that, const string& name, const string& key) {
        auto& v = ((ListNode*)that->node)->v;
        for (auto it = v.begin(); it != v.end(); ++it) {
            Accessor a(*it, that->owner);
            if (a[name].str() == key)
                return a;
        }
        FAIL("id not found");
    }
    static Accessor list_tryNodeWith(Accessor* that, const string& name, const string& key) {
        auto& v = ((ListNode*)that->node)->v;
        for (auto it = v.begin(); it != v.end(); ++it) {
            Accessor a(*it, that->owner);
            if (a[name].str() == key)
                return a;
        }
        return Accessor(nullptr, that->owner);
    }
    
    // ---------- Str
    static string str_str(Accessor* that); // this one is defined below since it depends on Yaml class
    static int str_len(Accessor* that) {
        auto& s = ((DataNode*)that->node)->str;
        return s.size;
    }

    // ---------- nums
    static double numdbl_dbl(Accessor* that) {
        return ((DataNode*)that->node)->num_dbl;
    }


    struct Ops {
        Accessor(*op_sq_int)(Accessor*, int);
        Accessor(*op_sq_str)(Accessor*, const string&);
        Accessor(*op_sq_chp)(Accessor*, const char*);
        Accessor(*nodeWith)(Accessor*, const string&, const string&);
        Accessor(*tryNodeWith)(Accessor*, const string&, const string&);
        string(*str)(Accessor*);
        double(*dbl)(Accessor*);
        int(*len)(Accessor*);
    };

    static const Ops& getOp(int type) {
        static const Ops ops[] = {
            { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
            { NULL, map_sq_str, map_sq_chp, NULL, NULL, NULL, NULL, map_len },
            { list_sq_int, NULL, NULL, list_nodeWith, list_tryNodeWith, NULL, NULL, list_len },
            { NULL, NULL, NULL, NULL, NULL, NULL, numdbl_dbl, NULL },
            { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
            { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
            { NULL, NULL, NULL, NULL, NULL, str_str, NULL, str_len },
        };
        return ops[type];
    }

};






template<typename T, int SZ>
class Pool
{
public:
    T* alloc() {
        if (m_curFill >= SZ) {
            m_cur = new T[SZ];
            m_arrs.push_back(m_cur);
            m_curFill = 0;
        }
        return &m_cur[m_curFill++];
    }

    vector<T*> m_arrs;
    int m_curFill = SZ+1;
    T* m_cur = nullptr;
};




bool isAlpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}
bool isSpace(char c) {
    return c == ' ' || c == '\t';
}
bool isWs(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}
bool isNum(char c) {
    return c >= '0' && c <= '9';
}

class Yaml
{
private:
    const char* m_buf;
    int m_pos;
    int m_size;

    Node* m_root = nullptr;
    int m_lastNewline;
    int m_lineCount;
    int m_lastListSize; // heuristic for the size of the next list to reserve

    friend struct Accessor;

    Pool<DataNode, 1240000> m_numPool;
public:
    Accessor root() {
        return Accessor(m_root, this);
    }
    void parse(const char* inbuf)
    {
        m_buf = inbuf;
        m_size = (int)strlen(inbuf);
        m_pos = 0;
        m_lastNewline = -1; // first newline is before the start
        m_lineCount = 1;
        m_lastListSize = 0;

        m_root = parseNode();
        CHECK(m_pos == m_size); // check we consumed everything
    }


    bool skipWs() {
        char c = m_buf[m_pos];
        while (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            if (c == '\n') {
                m_lastNewline = m_pos;
                ++m_lineCount;
            }
            ++m_pos;
            if (m_pos > m_size)
                return false;
            c = m_buf[m_pos];
            if (c == '#') { // skip comment
                while (c != '\n')
                    c = m_buf[++m_pos];
            }
        }
        return true;
    }

    
    Node* parseNode()
    {
        skipWs();
        char c = m_buf[m_pos];

        if (c == '&') { // node tag, just ignore the entire tag
            while (!isWs(c) && c != 0) 
                c = m_buf[++m_pos];
            skipWs();
            c = m_buf[m_pos];
        }

        // dashed list syntax, each element starts with '- ' but can also be '-\n' if the list is of lists
        if (c == '-' && m_size - m_pos > 2 && isWs(m_buf[m_pos + 1])) {
            auto ret = new ListNode;
            ret->type = NODE_LIST;
            int myindent = m_pos - m_lastNewline;  // include the -
            while (c == '-') {
                if (m_pos - m_lastNewline != myindent)
                    break;  // we arrived at a line of a different list
                ++m_pos; // skip -  
                auto *n = parseNode();
                ret->v.push_back(n);
                skipWs();  // skip the the next line to find the next -
                c = m_buf[m_pos];
            }
            return ret;
        }
        if (c == '[') { // inline list syntax
            auto ret = new ListNode;
            if (m_lastListSize != 0)
                ret->v.reserve(m_lastListSize);
            ret->type = NODE_LIST;
            while (true) {
                ++m_pos; // skip ,
                skipWs();  // there may be a space between , and next value
                c = m_buf[m_pos];  // will be checked after the loop
                if (c == ']') // the case of and empty list
                    break;
                auto *n = parseNode();
                ret->v.push_back(n);
                c = m_buf[m_pos];
                if (c != ',')
                    break;
            }
            CHECK(c == ']');
            ++m_pos; // skip ]
            m_lastListSize = (int)ret->v.size();
            return ret;
        }
        // otherwise it's a literal or a map key
        int sstart = m_pos;
        while (true) {
            c = m_buf[m_pos];
            if (isWs(c) || c == ':' || c == ',' || c == ']' || c == 0) {  // literal or key can terminate with these
                break;
            }
            ++m_pos;
        }
        Str s(m_buf + sstart, m_pos - sstart);

        skipWs();  // might be spaces after key and before :
        c = m_buf[m_pos];
        if (c == ':')  // it's the start of a map
        { 
            ++m_pos; // skip :
            MapNode* m = new MapNode;
            m->type = NODE_MAP;
            int myindent = sstart - m_lastNewline; // include the first letter
            Node* node = parseNode();  // first key was parsed, just need to value
            m->v[s] = node;
            while (true) // iterate key-values
            {
                skipWs();
                if (m_pos - m_lastNewline != myindent)
                    break;
                int kstart = m_pos;
                while (true) {
                    c = m_buf[m_pos];
                    if (isWs(c) || c == ':' || c == 0) {
                        break;
                    }
                    ++m_pos;
                }
                Str k(m_buf + kstart, m_pos - kstart);
                if (k.size == 0)
                    break;  // end of file reached
                skipWs();  // space after key name and before :
                c = m_buf[m_pos];
                CHECK(c == ':');
                ++m_pos;  // skip :
                Node* node = parseNode();
                m->v[k] = node;
            }
            return m;
        }
        // it's not a map

        c = m_buf[sstart];  // check if there'a a chance it's a number by how it starts
        if (isNum(c) || c == '-' || c == '.') {
            char* dend = nullptr;
            double d = my_strtod(m_buf + sstart, &dend);
            if (dend == s.end()) {
             //   auto *n = new DataNode;
                auto* n = m_numPool.alloc();
                n->type = NODE_NUM_DBL;
                n->num_dbl = d;
                return n;
            }
        }

        auto* n = new DataNode;
        n->type = NODE_STR;
        n->str.pos = (int)(s.start - m_buf);
        n->str.size = s.size;
        return n; // strEnd is the last one which was alpha so we want to end the string one after it
    }


};

inline string Accessor::str_str(Accessor* that) {
    auto& s = ((DataNode*)that->node)->str;
    return string(that->owner->m_buf + s.pos, s.size);
}









}