#pragma once

#include <map>
#include <vector>
#include <string>
#include <memory>

namespace ss_yaml {
using namespace std;




#define CHECK(cond) do { if (!(cond)) throw std::runtime_error("failed CHECK(" #cond ")"); } while(false)
#define FAIL(text) do { throw std::runtime_error(text); } while(false)

struct Str {
    Str(const char* s, int sz) : start(s), size(sz) {}
    explicit Str(const char* s) : start(s), size(strlen(s)) {}
    explicit Str(const string& s) : start(s.data()), size(s.size()) {}
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
    NODE_MAP,
    NODE_LIST,
    NODE_NUM,
    NODE_STR,
};

struct Node
{
    Node(ENodeType _type) : type(_type) {}
    virtual ~Node() {}
    virtual Node& operator[](int index) { FAIL("operator[int] not implemented for this node"); }
    virtual Node& operator[](const string& key) { FAIL("operator[str] not implemented for this node"); }
    virtual Node& operator[](const char* key) { FAIL("operator[char*] not implemented for this node"); }
    virtual Node& ofid(const string& key) { FAIL("ofid not implemented for this node"); }
    
    virtual string str() { FAIL("str not implemented for this node"); }
    virtual double dbl() { FAIL("flt not implemented for this node"); }
    virtual int len() { FAIL("len not implemented for this node"); }

    template<typename MatT, int sz>
    MatT mat()
    {
        CHECK(this->len() == sz);
        MatT ret;
        for (int i = 0; i < sz; ++i) {
            DsYaml::Node& line = (*this)[i];
            CHECK(line.len() == sz);
            for (int j = 0; j < sz; ++j)
                ret(i, j) = line[j].dbl();
        }
        return ret;
    }

    ENodeType type;
};

struct MapNode : public Node
{
    MapNode() : Node(NODE_MAP) {}
    virtual Node& operator[](const string& key) { 
        auto it = v.find(Str(key));
        CHECK(it != v.end());
        return *it->second;
    }
    virtual Node& operator[](const char* key) {
        auto it = v.find(Str(key));
        CHECK(it != v.end());
        return *it->second;
    }

    virtual int len() { 
        return v.size();
    }

    map<Str, Node*> v;
};

struct StrNode : public Node
{
    StrNode(const Str& s) : Node(NODE_STR), v(s) {}
    virtual string str() { return string(v.start, v.size); }
    virtual int len() {
        return v.size;
    }

    Str v;
};
struct NumNode : public Node
{
    NumNode(double _v) : Node(NODE_NUM), v(_v) {}
    virtual double dbl() { return v; }
    double v;
};
struct ListNode : public Node
{
    ListNode() : Node(NODE_LIST) {}
    virtual Node& operator[](int index) { 
        return *v[index];
    }
    virtual int len() {
        return v.size();
    }
    virtual Node& ofid(const string& id) {
        for (auto it = v.begin(); it != v.end(); ++it)
        {
            if ((**it)["id"].str() == id)
                return **it;
        }
        FAIL("id not found");
    }

    vector<Node*> v;
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
public:
    Node& root() {
        return *m_root;
    }
    void parse(const char* inbuf)
    {
        m_buf = inbuf;
        m_size = strlen(inbuf);
        m_pos = 0;
        m_lastNewline = -1; // first newline is before the start
        m_lineCount = 1;

        m_root = parseNode();
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

        if (c == '-' && m_size - m_pos > 2 && m_buf[m_pos + 1] == ' ') {
            auto ret = new ListNode();
            int myindent = m_pos - m_lastNewline;  // include the -
            while (c == '-') {
                if (m_pos - m_lastNewline != myindent)
                    break;  // we arrived at a line of a different list
                ++m_pos; // skip -  
                auto n = parseNode();
                ret->v.push_back(n);
                skipWs();  // skip the the next line to find the next -
                c = m_buf[m_pos];
            }
            return ret;
        }
        if (c == '[') { // inline list syntax
            auto ret = new ListNode();
            while (true) {
                ++m_pos; // skip ,
                skipWs();  // there may be a space between , and next value
                c = m_buf[m_pos];  // will be checked after the loop
                if (c == ']') // the case of and empty list
                    break;
                auto n = parseNode();
                ret->v.push_back(n);
                c = m_buf[m_pos];
                if (c != ',')
                    break;
            }
            CHECK(c == ']');
            ++m_pos; // skip ]
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
            double d = strtod(m_buf + sstart, &dend);
            if (dend == s.end())
                return new NumNode(d);
        }

        return new StrNode(s); // strEnd is the last one which was alpha so we want to end the string one after it
    }


};



}