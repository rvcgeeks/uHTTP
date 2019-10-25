# Rvc uHTTP Server

A 'Micro' HTTP server written from scratch in C

Features:
1) Basic MIME mapping
2) Basic directory listing
3) Low resource usage
4) Support Accept-Ranges: bytes (for in browser MP4 playing)
5) Concurrency by pre-fork

## Getting Started

These instructions will get you a copy of the project up and running on your local machine for development and testing purposes. See deployment for notes on how to deploy the project on a live system.

### Prerequisites

However this project is built on Ubuntu 16.04 LTS so
the release for this project can work on any 64 bit Debian distributions.
However we are planning to create a configuration for Windows in further releases

On the OS you need:
1) g++ >= 5.4.0

### Compilation

It has a build script so in terminal so just run

```
chmod +x build
./build
```
This will create a 'uhttp' server binary and immediately launch a readme webpage in htdocs in the firefox browser

## Deployment

run
```
./uhttp [max listeners] [port no]
```
To start the http server with 'max listeners' parallel listeners at port no 'port no'.
If no arguments are given the server starts at port 8000 with 20 listeners.

### Worldwide Deployment

Unlike the normal servers generally http, this server can serve only its clients as it may not or may give invalid response.
So to make it public you either can use a router NAT port forwarding from your router settings or use a dns service that
provides raw tcp data transfer what suits to you.  
As we have used gethostbyname client can connect the host

We had successfully tested this application with 'ngrok'
1) Create an account woth ngrok and follow their steps for installation and port forwarding. [Check their site](https://ngrok.com/download) 
2) Launch rvc-tcp-station server at say port X (>1024) 
3) Launch ngrok in plain tcp mode 
```
./ngrok http X
```
4) Now open the link given in terminal by ngrok in any of your favourite device and place

## Demo

![demo.gif](demo.gif)

## Author

* **Rajas Chavadekar** 

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details

