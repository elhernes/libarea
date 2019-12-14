// TextPath.cpp

// copyright Dan Heeks, September 3rd 2009, license: GPL version 3
// http://www.gnu.org/licenses/gpl-3.0.txt

#include "Writer.h"

#include <cmath>
#include <fstream>

GCodeWriter::GCodeWriter(const char *txt_file, int format_style) {
  m_ofs = new std::ofstream(txt_file);
  m_number_format = format_style;
  *m_ofs << "G20" << std::endl;
}

GCodeWriter::~GCodeWriter() { delete m_ofs; }

bool
GCodeWriter::Failed() {
  return !(*m_ofs);
}

static char str_for_num[1024];

static const char *
num1(double v) {
  int i = 0;
  if (v < 0)
    str_for_num[i] = '-';
  else
    str_for_num[i] = '+';
  i++;

  double value = v;
  value = fabs(value);
  if (value > 99999)
    value = 99999;

  double f = 10000.0;
  for (int j = 0; j < 8; j++) {
    double temp_v = value / f;
    if (j == 7)
      temp_v += 0.5; // round up last one
    int d = (int)temp_v;
    sprintf(&str_for_num[i], "%d", d);
    i++;
    value -= d * f;
    f *= 0.1;
    if (j == 4) {
      str_for_num[i] = '.';
      i++;
    }
  }

  // end string
  str_for_num[i] = 0;

  return str_for_num;
}

const char *
GCodeWriter::num(double v) {
  switch (m_number_format) {
  case 1:
    num1(v);
    break;

  default:
    sprintf(str_for_num, "%g", v);
    break;
  }

  return str_for_num;
}

void
GCodeWriter::OnRapidZ(double z) {
  (*m_ofs) << "G0Z" << num(z) << "\n";
}

void
GCodeWriter::OnRapidXY(double x, double y) {
  (*m_ofs) << "G0X" << num(x);
  (*m_ofs) << "Y" << num(y) << "\n";
}

void
GCodeWriter::OnFeedZ(double z) {
  (*m_ofs) << "G1Z" << num(z) << "\n";
}

void
GCodeWriter::OnFeedXY(double x, double y) {
  (*m_ofs) << "G1X" << num(x);
  (*m_ofs) << "Y" << num(y) << "\n";
}

void
GCodeWriter::OnArcCCW(double x, double y, double i, double j) {
  (*m_ofs) << "G3X" << num(x);
  (*m_ofs) << "Y" << num(y);
  (*m_ofs) << "I" << num(i);
  (*m_ofs) << "J" << num(j) << "\n";
}

void
GCodeWriter::OnArcCW(double x, double y, double i, double j) {
  (*m_ofs) << "G2X" << num(x);
  (*m_ofs) << "Y" << num(y);
  (*m_ofs) << "I" << num(i);
  (*m_ofs) << "J" << num(j) << "\n";
}
