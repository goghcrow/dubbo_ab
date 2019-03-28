## dubbo_ab beta

dubbo 命令行 工具, 支持泛化调用 dubbo 服务, 支持简单 qps 测试;

1. 支持 macOS 与 centos;
2. java 依赖版本(最低)
```
        <haunt-client.version>3.0.9-RELEASE</haunt-client.version>
        <bootstrap.version>3.1.2.2-RELEASE</bootstrap.version>
        <dubbo.version>3.1.2.1-RELEASE</dubbo.version>

```


3. 如果 mac 环境编译有循环引用问题

```shell
brew unlink libunistring
brew uninstall libunistring
Make sure /usr/local/include/stdint.h is gone, and delete it if necessary (I think it is)
brew install libunistring
```

编译使用

```
Usage:
   ./dubbo -h<HOST> -p<PORT> -m<METHOD> -a<JSON_ARGUMENTS> [-e<JSON_ATTACHMENT='{}'> -t<TIMEOUT_SEC=5> -c<CONCURRENCY> -n<REQUESTS> -v<VERBOS>]
```

注意参数使用方式, 不需要填写参数名称, 参数整体以数组方式传递, 参数value用相应 json 表示, e.g. java对象或者 map 使用 json 对象{}表示, list 使用 json 数组 [] 表示

[参数1, 参数2, ...]

```
./dubbo -h127.0.0.1 -p20881 -mcom.youzan.et.base.api.UserService.getAllUsers -a'[]'
./dubbo -h127.0.0.1 -p20881 -mcom.youzan.et.base.api.UserService.getUserMapByIds -a'[[14219614,14219615]]'
```