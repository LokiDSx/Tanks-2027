/**
 * \mainpage
 * \par Tanks
 * Battle City / Tank 1990 C++ implementation
 * \author Krystian Kałużny
 * \date 12.05.2015
 * \version 1.0
 * \par Kontakt:
 * \a k.kaluzny141@gmail.com
*/

#include "app.h"

#ifdef PSP_PORT
#include <pspkernel.h>
// Canonical PSP module descriptor: provides the .sceModuleInfo section that
// psp-fixup-imports needs to build a runnable EBOOT.PBP.
PSP_MODULE_INFO("TANKS2027", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
#endif

int main(int argc, char* args[] )
{
    App app;
    app.run();

    return 0;
}
