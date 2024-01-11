#include "material.h"
#include "tex3ds.h"
#include <string>
#include "console.h"
#include <unordered_map>
#include <memory>
#include <optional>
#include "texture.h"

namespace {
    struct t3xcfg_t {
        bool vram : 1 = false; // defaults to not in VRAM since less space there
        GPU_TEXTURE_FILTER_PARAM magFilter : 1 = GPU_LINEAR, minFilter: 1 = GPU_NEAREST;
        GPU_TEXTURE_WRAP_PARAM wrapS: 2 = GPU_REPEAT, wrapT: 2 = GPU_REPEAT;
    };
    std::unordered_map<std::string, Texture*> loadedTex;
    void texdeleter(Texture* tex) {
        if (tex) C3D_TexDelete(&tex->tex);
        delete loadedTex[tex->name];
        loadedTex.erase(tex->name);
        Console::log("texture deleted");
    }
}

material::~material() {}

std::optional<std::shared_ptr<Texture>> material::loadTextureFromFile(std::string name) {
    // Console::log("texture requested");
    if (name.size() == 0) name = "blank"; // no texture, load backup cat
    if (loadedTex.find(name) == loadedTex.end()) {
        FILE* f = fopen(("romfs:/gfx/" + name + ".t3x").c_str(), "r");
        if (!f) f = fopen("romfs:/gfx/kitten.t3x", "r"); // texture not found, load backup cat

        FILE* cfg = fopen(("romfs:/gfx/" + name + ".t3xcfg").c_str(), "r");
        
        t3xcfg_t texcfg;

        // const char *bit_rep[16] = {
        //     [ 0] = "0000", [ 1] = "0001", [ 2] = "0010", [ 3] = "0011",
        //     [ 4] = "0100", [ 5] = "0101", [ 6] = "0110", [ 7] = "0111",
        //     [ 8] = "1000", [ 9] = "1001", [10] = "1010", [11] = "1011",
        //     [12] = "1100", [13] = "1101", [14] = "1110", [15] = "1111",
        // };
        // if (name == "PlazaShopGlassTile_Alb")
        //     Console::log("%s%s", bit_rep[*(char*)&texcfg >> 4], bit_rep[*(char*)&texcfg & 0x0F]);

        if (cfg) fread(&texcfg, sizeof(t3xcfg_t), 1, cfg);

        Texture* tex = new Texture(name); 

        loadedTex[name] = tex;

        Tex3DS_Texture t3x = Tex3DS_TextureImportStdio(f, &tex->tex, &tex->cube, texcfg.vram);
        if (!t3x) return {};

        C3D_TexSetFilter(&tex->tex, texcfg.magFilter, texcfg.minFilter);
        C3D_TexSetWrap(&tex->tex, texcfg.wrapS, texcfg.wrapT);

        // Console::log("%p", *(char*)&texcfg);
        // if (name == "PlazaShopGlassTile_Alb")
        //     Console::log("%s%s", bit_rep[*(char*)&texcfg >> 4], bit_rep[*(char*)&texcfg & 0x0F]);

        
        // Delete the t3x object since we don't need it
        Tex3DS_TextureFree(t3x);
        // Close the files since we are done with them
        fclose(f);
        fclose(cfg);
    }
    return std::shared_ptr<Texture>(loadedTex[name], texdeleter); // if it already exists just return a pointer to it
}

bool material::loadTextureFromMem(C3D_Tex* tex, C3D_TexCube* cube, const void* data, size_t size)
{
    Tex3DS_Texture t3x = Tex3DS_TextureImport(data, size, tex, cube, false);
    if (!t3x)
        return false;

    // Delete the t3x object since we don't need it
    Tex3DS_TextureFree(t3x);
    return true;
}

int material::freadstr(FILE* fid, char* str, size_t max_size)
{
    int c;
    unsigned int count = 0;
    do {
        c = fgetc(fid);
        if (c == EOF) {
            /* EOF means either an error or end of file but
            * we do not care which. Clear file error flag
            * and return -1 */
            clearerr(fid);
            return -1;
        } else {
            /* Cast c to char */
            *str = (char) c;
            count++;
        }
    } while ((*(str++) != '\0') && (count < max_size));
    return count;
}