# POST Text and a File with cURL

Once the service is running a file may be posted with the cURL command line
tool.

To show the HTML form generated by the mdclean application:

    curl http://127.0.0.1:8080/mdclean/posttest

Then to make a POST request using some tags and a jpeg file in this
repository:

    curl -X POST -F 'tags=fuchsia purple pink' -F 'file=@testdata/fuchsia_magellanica.jpg' http://127.0.0.1:8080/mdclean/posttest

This mdclean application will take the data posted by cURL to nginx and save
it to a new temporary file which can be seen in the container's `/tmp`
directory:

    docker exec -i -t <container_name> /bin/ash
    /home/app # ls /tmp/
    mdclean_data.IMFBEG

The data in the `mdclean_data....` file is in mime format and must be decoded
with a mime decoder.

