// Path.h

// written by Dan Heeks, February 6th 2009, license: GPL version 3
// http://www.gnu.org/licenses/gpl-3.0.txt

#pragma once

#include <stdio.h>
#include <ostream>

class Writer {
public:
    virtual ~Writer() {};
    virtual void OnRapidZ(double z) = 0;
    virtual void OnRapidXY(double x, double y) = 0;
    virtual void OnFeedZ(double z) = 0;
    virtual void OnFeedXY(double x, double y) = 0;
    virtual void OnArcCCW(double x, double y, double i, double j) = 0;
    virtual void OnArcCW(double x, double y, double i, double j) = 0;
};

class GCodeWriter : public Writer {
    std::ostream *m_ofs;
    const char *num(double v);
    double m_zFeed;
    double m_xyFeed;

public:
    int m_number_format; // 0 is like "3.141529", 1 is like "+00003.141"

    GCodeWriter(const char *txt_file, int format_style);
    ~GCodeWriter();

    bool Failed();

  // CPath's virtual functions
    void OnRapidZ(double z) override;
    void OnRapidXY(double x, double y) override;
    void OnFeedZ(double z) override;
    void OnFeedXY(double x, double y) override;
    void OnArcCCW(double x, double y, double i, double j) override;
    void OnArcCW(double x, double y, double i, double j) override;
};

class DxfWriter : public Writer {
    FILE *m_fp;
    struct {
        double x;
        double y;
        double z;
    } m_pos;
public:
    DxfWriter(const char *txt_file);
    virtual ~DxfWriter();

  // CPath's virtual functions
    void OnRapidZ(double z) override;
    void OnRapidXY(double x, double y) override;
    void OnFeedZ(double z) override;
    void OnFeedXY(double x, double y) override;
    void OnArcCCW(double x, double y, double i, double j) override;
    void OnArcCW(double x, double y, double i, double j) override;
};
