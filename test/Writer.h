// Path.h

// written by Dan Heeks, February 6th 2009, license: GPL version 3
// http://www.gnu.org/licenses/gpl-3.0.txt

#pragma once

#include <ostream>

class Writer {
public:
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

public:
  int m_number_format; // 0 is like "3.141529", 1 is like "+00003.141"

  GCodeWriter(const char *txt_file, int format_style);
  ~GCodeWriter();

  bool Failed();

  // CPath's virtual functions
  void OnRapidZ(double z);
  void OnRapidXY(double x, double y);
  void OnFeedZ(double z);
  void OnFeedXY(double x, double y);
  void OnArcCCW(double x, double y, double i, double j);
  void OnArcCW(double x, double y, double i, double j);
};
