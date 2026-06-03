# Error codes in internal kernel APIs

- If function returns `int`, errno values are positive;  
- If function returns `ssize_t`, errno values are negative;  
- `lib/string.h` defines `strerror_r` which does `abs()` on the value;  

# Error codes in public kernel APIs
- All negative, syscall layer might want to convert those accordingly;  
