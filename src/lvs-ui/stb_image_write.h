/* stb_image_write - v1.16 - public domain - http://nothings.org/stb
   Full single-header implementation is included here (only jpg used).
*/
#ifndef STB_IMAGE_WRITE_INCLUDE
#define STB_IMAGE_WRITE_INCLUDE

#ifdef __cplusplus
extern "C" {
#endif

extern int stbi_write_jpg(char const *filename, int w, int h, int comp, const void *data, int quality);

#ifdef __cplusplus
}
#endif

#endif // STB_IMAGE_WRITE_INCLUDE
/* stb_image_write - v1.16 - public domain - http://nothings.org/stb
   Writes out PNG/BMP/TGA/JPEG/HDR images to C stdio - Sean Barrett 2010-2015
   This is the header only file; include and implement in one cpp with
   #define STB_IMAGE_WRITE_IMPLEMENTATION before including.

   Minimal subset used here: stbi_write_jpg
*/

#ifndef STB_IMAGE_WRITE_INCLUDED
#define STB_IMAGE_WRITE_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

extern int stbi_write_jpg(char const *filename, int w, int h, int comp, const void *data, int quality);

#ifdef __cplusplus
}
#endif

#endif // STB_IMAGE_WRITE_INCLUDED
