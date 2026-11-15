#ifndef MAXOS_SHELL_H
#define MAXOS_SHELL_H

/* argv/argc уже разобраны; возвращает код выхода (0 = ok) */
typedef int (*shell_cmd_fn)(int argc, char **argv);

void shell_register(const char *name, const char *help, shell_cmd_fn fn);
void shell_init(void);     /* регистрирует встроенные команды */
void shell_run(void);      /* основной цикл (не возвращаетcя до reboot/halt) */
void shell_banner(void);   /* печатает логотип */

#endif
