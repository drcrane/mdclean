`mdclean` A jpeg Metadata Cleaning Utility
==========================================

This project is an example of how to use FastCGI with nginx and C to run
`jhead` on an uploaded file and return the results.

Running the Application
-----------------------

### Local

The application requires the fcgi library and header files. These should
be available in most distributions and in Alpine it is `fcgi-dev`. Also
a local instance of some fcgi compatible server will need to be configured
for nginx see the settings in `nginx/nginx.conf.template` for an example.

    make appbin/mdclean
    spawn-fcgi -U nginx -G nginx -s /run/spawn-fcgi/mdclean.sock-1 -n ./mdclean

### Docker

    DOCKER_BUILDKIT=0 docker build --tag mdclean:0.0.2 --file Dockerfile .
    docker run --rm -i -t -p 8080:8080 -e PORT=8080 mdclean:0.0.2

