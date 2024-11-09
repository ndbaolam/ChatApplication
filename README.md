## Information
* Chat Application written in C

## Requirements
* Docker, Docker Compose
* Enviroment: Linux - Ubuntu

## Run Application
````
sudo apt-get install libpq-dev
docker compose up -d
gcc server.c -o server -I/usr/include/postgresql -L/usr/lib -lpq
./server
````
- Open ```http://localhost:3000``` in browser 