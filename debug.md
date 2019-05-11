# Debug tips

Kernel debug 这里相对比较容易，因为gdb启动后会解析kernel的ELF文件，能读取到kernel里面所有的symbol。

不过debug用户态程序时，由于用户态程序是kernel运行时加载的，一个比较合适的比喻是内核读取用户态的ELF文件，把里面的数据当程序运行。
_GDB自然不知道你的kernel运行了一个新的程序。_

为了便于用户态程序debug，可以在gdb启动后，手动添加symbol file，
```
add-symbol-file /path/to/your/symbol/file 0x800020
```
这里 `/path/to/your/symbol/file` 是你的ELF文件的位置，绝对路径和相对路径都可以。
`0x800020`是说你的symbol file中`.text`段的加载地址，从`obj/user/*.asm`中可以发现`.text`的加载地址。

接下来你就会发现，当你在运行用户态程序时，`backtrace` 显示的不再是一列`0x80**** () ?`这样的，能看到`libmain()`这些用户态程序的函数名、变量名了。:laughing:

**:question: add-symbol-file \*.o行不行**

也行，但是*.o文件里面只有对应源文件里的symbol，并且加载地址需要你根据ELF文件去确定（从*.asm文件里也能发现）。
相比上面加一个文件，这种方式需要你把所有相关的*.o文件都加进来，孰难孰易一看便知。