# tcmalloc
学习项目，参考gperftools实现的tcmalloc，有时间写一下关于tcmalloc的设计
# 测试
使用test中的speedtest的代码测试：
## 环境
- OS：Ubuntu 20.04 WSL

- 编译器：gcc9.3.0

- thread: 40; 

- Object Type: Small Object;
## 结果
### tcmalloc 

The total time of malloc: 30.34s

The total time of free: 18.25s

### malloc

The total time of malloc: 91.30s

The total time of free: 12.44s

