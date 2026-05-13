#ifndef AB66BCFE_D7D1_4116_8D38_EE7F447F0FD4
#define AB66BCFE_D7D1_4116_8D38_EE7F447F0FD4

enum {
    VPROT_NONE = 0x0000,  ///< No permissions
    VPROT_READ = 0x0001,  ///< Page can be read from
    VPROT_WRITE = 0x0002, ///< Page can be written to
    VPROT_EXEC = 0x0004,  ///< Page's code can be executed
    VPROT_USER = 0x0008,  ///< Page is accessible from user mode

    VPROT_RW = VPROT_READ | VPROT_WRITE,               ///< Kernel read/write
    VPROT_RWX = VPROT_READ | VPROT_WRITE | VPROT_EXEC, ///< Kernel read/write/execute
    VPROT_RX = VPROT_READ | VPROT_EXEC,                ///< Kernel read/execute

    VPROT_URW = VPROT_USER | VPROT_READ | VPROT_WRITE,               ///< Kernel/user read/write
    VPROT_URWX = VPROT_USER | VPROT_READ | VPROT_WRITE | VPROT_EXEC, ///< User read/write/execute
    VPROT_URX = VPROT_USER | VPROT_READ | VPROT_EXEC,                ///< User read/execute
};

#endif /* AB66BCFE_D7D1_4116_8D38_EE7F447F0FD4 */
