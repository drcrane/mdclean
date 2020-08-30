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

    make appsrc/mdclean
    spawn-fcgi -U nginx -G nginx -s /run/spawn-fcgi/mdclean.sock-1 -n ./mdclean

### Docker

    docker build --tag mdclean:0.0.1 --file Dockerfile .
    docker run --rm -i -t -e PORT=8080 mdclean:0.0.1

### Heroku

Using the Heroku CLI:

    heroko login -i
    heroku create --region eu
    < application_name returned >
    heroku container:login
    heroku container:push --app <application_name> web
    heroku container:release --app <application_name> web

