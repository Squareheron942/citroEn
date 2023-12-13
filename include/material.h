#pragma once

#include "color.h"
#include "citro3d.h"
#include <stdio.h>

class material {
    protected:
    static int freadstr(FILE* fid, char* str, size_t max_size);
    static bool loadTextureFromMem(C3D_Tex* tex, C3D_TexCube* cube, const void* data, size_t size);
    static bool loadTextureFromFile(C3D_Tex* tex, C3D_TexCube* cube, const char* path, bool vram);
    public:
    material() noexcept {};
    virtual ~material() = 0;
    virtual void resetMaterial() = 0;
    virtual void setMaterial(C3D_Mtx *modelView, C3D_Mtx* projection) = 0;
};