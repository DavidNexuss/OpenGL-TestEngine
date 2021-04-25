#include <glm/glm.hpp>
#include <list>

struct SpatialNode
{
    std::list<SpatialNode*> children;
    SpatialNode* parent;

    glm::mat4 combined;
    glm::mat4 transform;

    size_t cacheFrame = 0;

    SpatialNode(SpatialNode* _parent,glm::mat4 _transform) : parent(_parent), transform(_transform) { }

    inline glm::mat4 getCombined(size_t _cacheFrame)
    {
        if (cacheFrame) return combined;
        combined = parent ? parent->getCombined(_cacheFrame) * transform : transform;
        cacheFrame = _cacheFrame;
        return combined;
    }

    inline void flush()
    {
        cacheFrame = 0;
        for(auto &child : children) child->flush();
    }

    inline bool isNew(size_t _cacheFrame) const
    {
        return cacheFrame == 0 || cacheFrame == _cacheFrame;
    }
};

struct Spatial
{
    SpatialNode* node;

    Spatial(const glm::mat4& transform) : node(new SpatialNode(nullptr,transform)) { }
    Spatial(const glm::mat4& transform,const Spatial& parent) : node(new SpatialNode(parent.node,transform)) { }
    Spatial(const Spatial& other):  node(new SpatialNode(other.node->parent,other.node->transform)) { }

    operator glm::mat4() const { return node->transform; }

    inline SpatialNode* getNode() { return node; }

    void setParent(const Spatial& parent) const 
    { 
        node->parent = parent.node;
        parent.node->children.push_back(node);
    }

    Spatial& operator=(const glm::mat4& transform) { node->transform = transform; node->cacheFrame = 0; return *this; }
    Spatial& operator=(const Spatial& other) { node = other.node; return *this; }
};
