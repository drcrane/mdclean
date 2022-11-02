# Jpeg Metadata Cleaning Utility `mdclean`

This project is an example of how to use FastCGI with nginx and C to run
`jhead` on an uploaded file and return the results.

## Running the Application

### Dependencies

This application makes use of some library functions from `libgreen`
which is a small collection of useful C and C++ functions and algorithms.

### Local

The application requires the fcgi library and header files. These should
be available in most distributions and in Alpine it is `fcgi-dev`. Also
a local instance of some fcgi compatible server will need to be configured
for nginx see the settings in `nginx/nginx.conf.template` for an example.

    make appsrc/mdclean
    spawn-fcgi -U nginx -G nginx -s /run/spawn-fcgi/mdclean.sock-1 -n appsrc/mdclean

### Docker

The `Dockerfile` included in the distribution includes a build step which
allows the project to be compiled and the image created in a single step.

    docker build --tag mdclean:0.0.3 --file Dockerfile .
    docker run --rm -i -t -p 8080:8080 -e PORT=8080 mdclean:0.0.3

## Using the Application

Vist the web page containing the form allowing a file to be uploaded:

    http://127.0.0.1/mdclean/posttest

The host `127.0.0.1` and the first part of the path should be altered to
fit your specific infrastructure and nginx configuration.

Select a small file file and click the `Upload` button. Your browser
should then begin a file download, this file is the file that you selected
to upload. In version 0.0.3 (this version) the file will simply be sent
back but the application will hint to the browser that the file should be
downloaded. The hint is through a header `Content-Disposition` see
`mdclean.c#process_posted_data`.

