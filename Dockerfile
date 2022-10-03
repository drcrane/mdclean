FROM alpine:3.13

WORKDIR /home/build

RUN apk add --no-cache build-base fcgi-dev

COPY Makefile .
ADD appsrc/*.c appsrc/

RUN make appsrc/mdclean

FROM alpine:3.13

WORKDIR /home/app

RUN apk add --no-cache s6 nginx fcgi spawn-fcgi && \
	rm -r /etc/nginx/conf.d /etc/nginx/modules && \
	rm -r /etc/nginx/fastcgi.conf /etc/nginx/scgi_params /etc/nginx/uwsgi_params
ADD s6 /s6
COPY nginx/nginx.conf.template /etc/nginx/nginx.conf.template
COPY htdocs /var/www/localhost/htdocs/

COPY --from=0 /home/build/appsrc/mdclean .

CMD ["s6-svscan", "/s6"]

