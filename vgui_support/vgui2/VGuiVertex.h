#ifndef VGUIVERTEX_H
#define VGUIVERTEX_H

namespace vgui2
{

class VGuiVertex {
public:
    VGuiVertex() : x(0), y(0), u(0.0f), v(0.0f) {}
    VGuiVertex(int x, int y, float u, float v) : x(x), y(y), u(u), v(v) {}
    
    void SetVertex(int _x, int _y)
    {
        x = _x;
        y = _y;
    }

    void SetVertex(int _x, int _y, float _u, float _v)
    {
        x = _x;
        y = _y;
        u = _u;
        v = _v;
    }

    int GetX() { return x; }
    int GetY() { return y; }
    float GetU() { return u; }
    float GetV() { return v; }

    void GetVertex(int &_x, int _y, float &_u, float &_v)
    {
        _x = x;
        _y = y;
        _u = u;
        _v = v;
    }

    bool operator==(VGuiVertex &o) const
    {
        return x == o.x && y == o.y && u == o.u && v == o.v;
    }

private:
    int x;
    int y;
    float u;
    float v;
};

}

#endif // VGUIVERTEX_H
