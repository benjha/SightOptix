/*
 * Distributed under the OSI-approved Apache License, Version 2.0.  See
 * accompanying file Copyright.txt for details.
 */

#ifndef CCOLOR_H_
#define CCOLOR_H_

// based on EAVL's color
#include <iostream>

using namespace std;

static inline float MapValueToNorm(double value,
                                   double vmin,
                                   double vmax,
                                   bool logarithmic);
class cColor
{
  public:
    float c[4];
    cColor()
    {
        c[0] = 0;
        c[1] = 0;
        c[2] = 0;
        c[3] = 1;
    }
    cColor(float r_, float g_, float b_, float a_=1)
    {
        c[0] = r_;
        c[1] = g_;
        c[2] = b_;
        c[3] = a_;
    }
    inline void SetComponentFromByte(int i, unsigned char v)
    {
        // Note that though GetComponentAsByte below
        // multiplies by 256, we're dividing by 255. here.
        // This is, believe it or not, still correct.
        // That's partly because we always round down in
        // that method.  For example, if we set the float
        // here using byte(1), /255 gives us .00392, which
        // *256 gives us 1.0035, which is then rounded back
        // down to byte(1) below.  Or, if we set the float
        // here using byte(254), /255 gives us .99608, which
        // *256 gives us 254.996, which is then rounded
        // back down to 254 below.  So it actually reverses
        // correctly, even though the mutliplier and
        // divider don't match between these two methods.
        //
        // Of course, converting in GetComponentAsByte from
        // 1.0 gives 256, so we need to still clamp to 255
        // anyway.  Again, this is not a problem, because it
        // doesn't really extend the range of floating point
        // values which map to 255.
        c[i] = float(v) / 255.;
        // clamp?
        if (c[i]<0) c[i] = 0;
        if (c[i]>1) c[i] = 1;
    }
    inline unsigned char GetComponentAsByte(int i)
    {
        // We need this to match what OpenGL/Mesa do.
        // Why?  Well, we need to set glClearColor
        // using floats, but the frame buffer comes
        // back as bytes (and is internally such) in
        // most cases.  In one example -- parallel
        // compositing -- we need the byte values
        // returned from here to match the byte values
        // returned in the frame buffer.  Though
        // a quick source code inspection of Mesa
        // led me to believe I should do *255., in
        // fact this led to a mismatch.  *256. was
        // actually closer.  (And arguably more correct
        // if you think the byte value 255 should share
        // approximately the same range in the float [0,1]
        // space as the other byte values.)  Note in the
        // inverse method above, though, we still use 255;
        // see SetComponentFromByte for an explanation of
        // why that is correct, if non-obvious.

        int tv = c[i] * 256.;
        // Converting even from valid values (i.e 1.0)
        // can give a result outside the range (i.e. 256),
        // but we have to clamp anyway.
        return (tv < 0) ? 0 : (tv > 255) ? 255 : tv;
    }
    void GetRGBA(unsigned char &r, unsigned char &g,
                 unsigned char &b, unsigned char &a)
    {
        r = GetComponentAsByte(0);
        g = GetComponentAsByte(1);
        b = GetComponentAsByte(2);
        a = GetComponentAsByte(3);
    }
    double RawBrightness()
    {
        return (c[0]+c[1]+c[2])/3.;
    }
    friend ostream &operator<<(ostream &out, const cColor &c)
    {
        out << "["<<c.c[0]<<","<<c.c[1]<<","<<c.c[2]<<","<<c.c[3]<<"]";
        return out;
    }
/*
    static cColor black;
    static cColor grey10;
    static cColor grey20;
    static cColor grey30;
    static cColor grey40;
    static cColor grey50;
    static cColor grey60;
    static cColor grey70;
    static cColor grey80;
    static cColor grey90;
    static cColor white;

    static cColor red;
    static cColor green;
    static cColor blue;

    static cColor cyan;
    static cColor magenta;
    static cColor yellow;
    */
};

/*
cColor cColor::black (0.0, 0.0, 0.0);
cColor cColor::grey10(0.1, 0.1, 0.1);
cColor cColor::grey20(0.2, 0.2, 0.2);
cColor cColor::grey30(0.3, 0.3, 0.3);
cColor cColor::grey40(0.4, 0.4, 0.4);
cColor cColor::grey50(0.5, 0.5, 0.5);
cColor cColor::grey60(0.6, 0.6, 0.6);
cColor cColor::grey70(0.7, 0.7, 0.7);
cColor cColor::grey80(0.8, 0.8, 0.8);
cColor cColor::grey90(0.9, 0.9, 0.9);
cColor cColor::white (1.0, 1.0, 1.0);

cColor cColor::red    (1.0, 0.0, 0.0);
cColor cColor::green  (0.0, 1.0, 0.0);
cColor cColor::blue   (0.0, 0.0, 1.0);

cColor cColor::cyan   (0.0, 1.0, 1.0);
cColor cColor::magenta(1.0, 0.0, 1.0);
cColor cColor::yellow (1.0, 1.0, 0.0);
*/


#endif /* CCOLOR_H_ */
