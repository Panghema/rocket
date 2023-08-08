# Rocket
一个简易的c++ RPC框架
参考项目:[rocket by ikerli](https://github.com/Gooddbird/rocket)

## rocket/common
### config
- 使用tinyxml库解析日志级别、日志名、日志地址、单个日志文件最大大小、异步记录日志事件间隔、端口号、线程数量。
- 通过setglobalconfig函数设置静态指针全局共同使用，并以getglobalconfig函数在需要使用的时候获取。
### log
- 日志等级分为以下四个等级，并支持String与int（枚举）的转换
    - Unknown（0）
    - Debug（1）
    - Info（2）
    - Error（3）
- 异步记录日志的过程即每个工作线程把日志push到一个buffer，再启动一个线程专门用以将buffer中的日志内容落盘
