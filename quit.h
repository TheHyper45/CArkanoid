#ifndef QUIT_H
#define QUIT_H

void SetError(const char* fmt,...);
const char* GetError(void);
_Noreturn void ExitApplication(void);
_Noreturn void AbortApplication(const char* fmt,...);

#endif