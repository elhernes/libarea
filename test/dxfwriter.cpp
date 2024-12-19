/***************************************************
 * file: machining/libarea~2/test/dxfwriter.cpp
 *
 * @file    dxfwriter.cpp
 * @author  Eric L. Hernes
 * @version V1.0
 * @born_on   Tuesday, June 23, 2020
 * @copyright (C) Copyright Eric L. Hernes 2020
 * @copyright (C) Copyright Q, Inc. 2020
 *
 * @brief   An Eric L. Hernes Signature Series C++ module
 *
 */

#include <stdint.h>
#include <stdio.h>
#include "Writer.h"

DxfWriter::DxfWriter(const char *txt_file) {
    m_fp = fopen(txt_file, "w");
    m_pos.x=0.;
    m_pos.y=0.;
    m_pos.z=0.;
}

DxfWriter::~DxfWriter() {
    fclose(m_fp);
}

void
DxfWriter::OnRapidZ(double z) {
    m_pos.z=z;
}

void
DxfWriter::OnRapidXY(double x, double y) {
    m_pos.x=x;
    m_pos.y=y;
}

void
DxfWriter::OnFeedZ(double z) {
}

void
DxfWriter::OnFeedXY(double x, double y) {
}

void
DxfWriter::OnArcCCW(double x, double y, double i, double j) {
}

void
DxfWriter::OnArcCW(double x, double y, double i, double j) {
}

/* end of machining/libarea~2/test/dxfwriter.cpp */
