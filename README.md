## dubbo_ab beta

dubbo 命令行 工具, 支持泛化调用 dubbo 服务, 支持简单 qps 测试;

1. 支持 macOS 与 centos;
2. java 依赖版本
```
        <haunt-client.version>3.0.9-RELEASE</haunt-client.version>
        <bootstrap.version>3.1.2.2-RELEASE</bootstrap.version>
        <dubbo.version>3.1.2.1-RELEASE</dubbo.version>

```

```
make

Usage:
   ./dubbo_test -h<HOST> -p<PORT> -m<METHOD> -a<JSON_ARGUMENTS> [-e<JSON_ATTACHMENT='{}'> -t<TIMEOUT_SEC=5> -c<CONCURRENCY> -n<REQUESTS> -v<VERBOS>]
```