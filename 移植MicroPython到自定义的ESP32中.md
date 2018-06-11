# 移植MicroPython到自定义的ESP32中

**一、了解现状**

​	MicroPython是一种嵌入式的Python3语言解释器，与此相近的还有CPython，但不同的是，它对现有的Pyhton库支持较为薄弱，但在性能和效率上控制的很好，与CPython相比更适合底层硬件的运行环境。

​	它有哪些优点及缺陷？

​	首先它相对于C/C++的静态编译而言，它是动态执行的，其次它相对于C/C++编程环境更为简单和易用，尤其是内置了许多常见的数据结构，对开发人员或数学研究人员，构建或移植相关数学算法更为容易。接着动态解释器有利于程序运行时修改，对于一段硬件控制输出不确定的PWM的功能，可以在Python中重新执行，而非重新烧写固件到芯片中运行，在未来由于嵌入式硬件的完善和升级，开发者会期望将一些软件功能从上层迁移到下层时，MicroPython可以提供比C/C++更为简易通用跨平台语言环境。

​	缺陷很直接，对芯片内存占用较大，因为其开放了所有硬件接口功能和之间存在语言转换层，MicroPython对内存空间有一定要求，面对复杂而高级的软件功能，其开发相当考验开发人员的水平，如果仅是重复性硬件操作，MircoPython的开发效率远远比C/C++更高。

​	如果担心其在面对更高性能场景的时候力不从心，不用担心，MicroPython允许内嵌C/C++模块重编译进入Python解释器环境当中，从Python回到C/C++的执行环境中仅占用O（4）的时间复杂度。

**二、了解架构**

​	先介绍一些移植芯片的架构情况，也就是ESP32的IDF，简单来说，现阶段的Arduino或MicroPython的移植都是基于此环境而来，这也是ESP32的底层标准环境，在这一层中使用的C++的编译环境，允许其使用C++的语法功能，因此Arduino基于IDF提供封装的静态C++硬件接口类，以及Arduino的软件核心C++泛型类。

​	而MicroPython不同的是在解释器这个环节中是使用C语言编写的，所以针对C和C++使用了多个编译器，其中还包括mpy文件的交叉编译等等。

​	所以我们需要明确的是，MicroPython虽然是基于IDF的，但并不意味着它可以直接调用IDF环境，这需要在IDF当中完成后再迁移至MicroPython，并且是标准C实现的，也就是说，不能将Arduino的类拿到MicroPython当中直接调用（针对于C++的类）。

​	架构的介绍说明完了之后，着重于细节说明。

​	回到IDF的部分，对直接应用的开发人员来说，仅需关注[components](https://github.com/yelvlab/BPI-BIT-MPy/tree/master/esp-idf/components)部分，这里提供了ESP32的所有硬件功能的源码，你可以选择性的添加所需的代码到MircoPython中其中，这都是可行的，可以无视官方makefile所要求的指定IDF版本。

​	对于MircoPython而言，IDF就形如一个标准库，也就是说，在MircoPython中是调用原生的硬件库代码映射到内部的Python语言层代码的，而它又是如何编译和移植的呢？我们需要关注的是其根目录下的[ports](https://github.com/yelvlab/BPI-BIT-MPy/tree/master/micropython/ports)，在这里有着各种硬件的移植，比如Esp32和Windows文件夹就是对应系统下的移植，比较简单可见的是Windows的移植，仅需通过VS2013以上的IDE打开编译即可得到MircoPython解释器并运行代码。

​	而Esp32同理，不同的是需要在Linux通过交叉编译才可以编译生成最终固件的，详细内容将在下一章节说明。

​	此外，简要的说明一下MircoPython的架构布局。

| **[py](https://github.com/yelvlab/BPI-BIT-MPy/tree/master/micropython/py)** | **提供了Python解释器的实现，像一些Py到C的类型转换、Py内置的数据结构以及Py解释器的特有功能，例如内存回收，内置函数等。** |
| ------------------------------------------------------------ | ------------------------------------------------------------ |
| **[extmod](https://github.com/yelvlab/BPI-BIT-MPy/tree/master/micropython/extmod)** | **通过 C 实现的硬件模块，主要是抽象统一硬件外设库的实现，不能抽象出来的硬件外设都放在了各自的Ports对应的芯片型号文件夹之中。** |
| **[lib](https://github.com/yelvlab/BPI-BIT-MPy/tree/master/micropython/lib)** | **标准C实现的一些库代码，例如文件系统的oofatfs，该部分均是可以跨平台实现的代码** |
| **[drivers](https://github.com/yelvlab/BPI-BIT-MPy/tree/master/micropython/drivers)** | **针对一些硬件外设的传感器，尤其是一些芯片特有的，均在此处实现Py与C结合的驱动，这个部分不一定被所有芯片支持，举例来说，提供一个dht温度传感器的Py库实现，将分别提供C到Py的接口转换函数和直接Py上使用的库函数接口，对用户而言仅需调用库即可，其他硬件接口的移植到Py均同理，但有可能不会统一放至该处，而是放在Ports的对应芯片文件夹当中。** |

​	注：ESP32是基于ESP8266所移植得来，所以不能读懂ESP32的设计的时候可以回到ESP8266来学习了解，同样的，在实际应用的时候，[官方文档](http://docs.micropython.org/en/latest/esp8266/)是针对ESP8266所撰写的，所以没有对应ESP32的开发文档。

**三、着手移植**

​	如果以windows为例可以很简单的完成移植，但对于Esp32则需要准备编译器的执行环境了，并且我不推荐在Windows上编译，因为makefile中有一些linux特有的命令在windows上需要配置才有。

​	以我的环境为例，先是准备了一个Linux环境（Lubuntu），上面提供了最基本的Gcc编译以及[xtensa-esp32-elf](https://github.com/yelvlab/BPI-BIT-MPy/tree/master/xtensa-esp32-elf)的Esp编译工具，至于这一块，我们可以参考[Micropython加速物联网开发7 - Micropython源码编译与固件更新](https://blog.csdn.net/messidona11/article/details/71707776?utm_source=gold_browser_extension)一文下载所需的功能，常见的缺失问题均可通过`sudo apt-get install 软件名`来解决。

​	假设已经确定make命令可用了，就可以尝试编译MicroPython固件了，如果存在问题，请跳转至**五**章节进行疑难解惑，我将一一收集问题并解答。

​	那么该如何编译固件？

​	我们看到BPI-BIT-MPy的[README](https://github.com/yelvlab/BPI-BIT-MPy/blob/master/README.md)，这里是简单的说明一下，给自己的linux设置环境变量，主要有`PATH=$PATH:/root/esp32/xtensa-esp32-elf/bin`和`IDF_PATH=/root/esp32/esp-idf`，这个路径是和自己linux系统有关的，我只是习惯在root目录下进行操作了，所以此处的主要路径仅与esp32文件夹路径有关。
​	当你能够确定你的环境路径已经设置成功以后，如果不能请自行查阅有关于环境变量的其他资料。现在可以就可以到[esp32](https://github.com/yelvlab/BPI-BIT-MPy/tree/master/micropython/ports/esp32)下尝试make了，成功了将不会出现红色或黄色的字体XD，如果不成功请继续到疑难解惑里查阅问题或留言。

**四、修改源码**

​	先说明一下官方的源码情况，在官方的ESP32的应用案例中，是默认将ESP32的串口当作控制台解释器接口，也就是和计算机上的控制台窗口程序交互一样，这是因为官方的编辑器的开发方式就是直接的代码解释交互，除了非必要的静态代码会存储在BOOT.PY，其他的代码执行会交给官方开发工具在线编程。
​	而我的修改目标是脱离官方的开发模式，将开发的代码方式转换成云端控制的方式，也就不需要本地的串口进行代码交互，将代码通过WebDav的方式下载到芯片空间上，仅需

**五、疑难解惑**

​	所有遇到的问题将在此处收集，存在重点部分的问题可能将交由。
​	
