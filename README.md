Lightweight pid1 for Docker containers
======================================

`pid1` is a very simple implementation of linux init system, intended to be 
used with light Docker containers, solving the [zombie reaping problem](
https://blog.phusion.nl/2015/01/20/docker-and-the-pid-1-zombie-reaping-problem/).

Based on [Rich Felker's proposal](http://ewontfix.com/14/) for correct PID1 process.

Use this as a container entrypoint. The actual main program of the container 
is passed as an argument.

Example `Dockerfile`:
```Dockerfile
FROM debian:jessie
...
CMD ['/bin/pid1', '/path/to/myapp', 'arg1', 'arg2']
```
