#pragma once

#include <map>
#include <vector>
#include <string>
#include <memory>

namespace DsYaml {
using namespace std;




#define CHECK(cond) do { if (!(cond)) throw std::runtime_error("failed CHECK(" #cond ")"); } while(false)
#define FAIL(text) do { throw std::runtime_error(text); } while(false)

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
    virtual Node& ofid(const string& key) { FAIL("ofid not implemented for this node"); }
    
    virtual string& str() { FAIL("str not implemented for this node"); }
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
        auto it = v.find(key);
        CHECK(it != v.end());
        return *it->second;
    }

    virtual int len() { 
        return v.size();
    }

    map<string, Node*> v;
};
struct StrNode : public Node
{
    StrNode(const string& _v) : Node(NODE_STR), v(_v) {}
    virtual string& str() { return v; }
    virtual int len() {
        return v.size();
    }

    string v;
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
bool isNum(char c) {
    return c >= '0' && c <= '9';
}

class Yaml
{
private:
    const char* m_buf;
    int m_pos;
    int m_size;

    MapNode* m_root = nullptr;
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

        m_root = parseMap();
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


    MapNode* parseMap()
    {
        MapNode* m = new MapNode;
        int myindent = m_pos - m_lastNewline; // include the first letter
        while (true) // iterate key-values
        {
            //if (!skipWs())
            //    break;
            char c = m_buf[m_pos];
            if (isAlpha(c)) {
                if (m_pos - m_lastNewline != myindent)
                    break;
                int keyStart = m_pos;
                while (isAlpha(c)) {
                    c = m_buf[++m_pos];
                }
                int keyEnd = m_pos;
                skipWs();
                c = m_buf[m_pos];
                CHECK(c == ':');
                ++m_pos;  // skip :
                skipWs();
                string key(m_buf + keyStart, m_buf + keyEnd);
                Node* node = parseNode();
                m->v[key] = node;
                skipWs();
            }
            else
                break; // it's something other than a key again
        }
        return m;
    }
    
    Node* parseNode()
    {
        char c = m_buf[m_pos];
        // first check if it's a literal string of number
        if (isAlpha(c) || isNum(c) || (c == '-' && isNum(m_buf[m_pos + 1])) ) {
            // look ahead to see if there is a ':' - then it's a map
            int lookpos = m_pos;
            int inStr = true;
            int strEnd = -1;
            bool hasAlpha = false;
            while (true) {
                c = m_buf[lookpos];
                if (inStr && (isAlpha(c) || c == '-' || isNum(c) || c == '-' || c == '.')) {  // string and keys can have hyphens or numbers in them
                    hasAlpha |= (isAlpha(c) && c != 'e');
                    strEnd = lookpos;
                    ++lookpos;
                    continue;
                }
                inStr = false;
                if (isSpace(c)) {  // there can be spaces after the key and before the :
                    ++lookpos;
                    continue;
                }
                break;
            }
            if (c == ':')  // it's a map
                return parseMap();

            Node* n = nullptr;
            if (hasAlpha)
                n = new StrNode(string(m_buf + m_pos, m_buf + strEnd + 1)); // strEnd is the last one which was alpha so we want to end the string one after it
            else {
                char* dend = nullptr;
                double d = strtod(m_buf + m_pos, &dend);
                CHECK(dend == m_buf + strEnd + 1);
                n = new NumNode(d);
            }
            m_pos = strEnd + 1;
            skipWs();
            return n;
        }

        if (c == '-') {
            auto ret = new ListNode();
            int myindent = m_pos - m_lastNewline;  // include the -
            while (c == '-') {
                if (m_pos - m_lastNewline != myindent)
                    break;  // we arrived at a line of a different list
                ++m_pos; // skip -  
                skipWs();
                auto n = parseNode();
                ret->v.push_back(n);
                skipWs();
                c = m_buf[m_pos];
            }
            return ret;
        }
        if (c == '[') { // inline list syntax
            auto ret = new ListNode();
            while (true) {
                ++m_pos; // skip - or ,
                skipWs();
                c = m_buf[m_pos];
                if (c == ']') // the case of and empty list
                    break;
                auto n = parseNode();
                ret->v.push_back(n);
                c = m_buf[m_pos];
                if (c != ',')
                    break;
            }
            CHECK(c == ']');
            ++m_pos;
            return ret;
        }
        CHECK(false);
    }


};



}