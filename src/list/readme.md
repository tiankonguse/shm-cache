## Writing C header files

C++ compilers require that functions be declared with full prototypes, since C++ is more strongly typed than C.   
C functions and variables also need to be declared with the extern "C" directive, so that the names aren't mangled.

ANSI C compilers are not as strict as C++ compilers, but functions should be prototyped to avoid unnecessary warnings when the header file is #included.  
Non-ANSI compilers will report errors if functions are prototyped.  


These complications mean that your library interface headers must use some C preprocessor magic in order to be usable by each of the above compilers.  

Here are the relevant portions of that file:

```
/* __BEGIN_DECLS should be used at the beginning of your declarations,
   so that C++ compilers don't mangle their names.  Use __END_DECLS at
   the end of C declarations. */
#undef __BEGIN_DECLS
#undef __END_DECLS
#ifdef __cplusplus
# define __BEGIN_DECLS extern "C" {
# define __END_DECLS }
#else
# define __BEGIN_DECLS /* empty */
# define __END_DECLS /* empty */
#endif

/* __P is a macro used to wrap function prototypes, so that compilers
   that don't understand ANSI C prototypes still work, and ANSI C
   compilers can issue warnings about type mismatches. */
#undef __P
#if defined (__STDC__) || defined (_AIX) \
        || (defined (__mips) && defined (_SYSTYPE_SVR4)) \
        || defined(WIN32) || defined(__cplusplus)
# define __P(protos) protos
#else
# define __P(protos) ()
#endif
```




## __builtin_prefetch


```
void __builtin_prefetch (const void *addr, ...)
```

This function is used to minimize cache-miss latency by moving data into a cache before it is accessed.   
You can insert calls to __builtin_prefetch into code for which you know addresses of data in memory that is likely to be accessed soon.   
If the target supports them, data prefetch instructions will be generated.   
If the prefetch is done early enough before the access then the data will be in the cache by the time it is accessed.


The value of addr is the address of the memory to prefetch.   
There are two optional arguments, rw and locality.   

The value of rw is a compile-time constant one or zero;   
one means that the prefetch is preparing for a write to the memory address and zero, the default, 
means that the prefetch is preparing for a read. 

The value locality must be a compile-time constant integer between zero and three.   
A value of zero means that the data has no temporal locality, so it need not be left in the cache after the access.   
A value of three means that the data has a high degree of temporal locality and should be left in all levels of cache possible.   
Values of one and two mean, respectively, a low or moderate degree of temporal locality. 
The default is three.  


```
for (i = 0; i < n; i++){
    a[i] = a[i] + b[i];
    __builtin_prefetch (&a[i+j], 1, 1);
    __builtin_prefetch (&b[i+j], 0, 1);
    /* ... */
}
```

Data prefetch does not generate faults if addr is invalid, but the address expression itself must be valid.   
For example, a prefetch of p->next will not fault if p->next is not a valid address, but evaluation will fault if p is not a valid address.

If the target does not support data prefetch, the address expression is evaluated if it includes side effects but no other code is generated and GCC does not issue a warning.


