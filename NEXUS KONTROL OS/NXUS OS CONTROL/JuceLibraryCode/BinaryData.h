/* =========================================================================================

   This is an auto-generated file: Any edits you make may be overwritten!

*/

#pragma once

namespace BinaryData
{
    extern const char*   slider2_png;
    const int            slider2_pngSize = 4300;

    extern const char*   slider_png;
    const int            slider_pngSize = 619;

    extern const char*   slider3_png;
    const int            slider3_pngSize = 16986;

    extern const char*   LevelScaleComponent_cpp;
    const int            LevelScaleComponent_cppSize = 2450;

    extern const char*   cpu_load_h;
    const int            cpu_load_hSize = 2746;

    extern const char*   fount_label_h;
    const int            fount_label_hSize = 2395;

    // Number of elements in the namedResourceList and originalFileNames arrays.
    const int namedResourceListSize = 6;

    // Points to the start of a list of resource names.
    extern const char* namedResourceList[];

    // Points to the start of a list of resource filenames.
    extern const char* originalFilenames[];

    // If you provide the name of one of the binary resource variables above, this function will
    // return the corresponding data and its size (or a null pointer if the name isn't found).
    const char* getNamedResource (const char* resourceNameUTF8, int& dataSizeInBytes);

    // If you provide the name of one of the binary resource variables above, this function will
    // return the corresponding original, non-mangled filename (or a null pointer if the name isn't found).
    const char* getNamedResourceOriginalFilename (const char* resourceNameUTF8);
}
