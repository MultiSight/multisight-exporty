Exporty

Exporty can be thought of as a query front end for the Recorder that offers data in standard formats.

By default, Exporty is configured to talk to Recorder instance on localhost. Exporty's API supports jpg thumbnails, containerization
into standard formats (mp4) and transcoding.

Here is an example of a thumbnail query:

http://127.0.0.1:10013/thumbnail?data_source_id=0d022027-0000-0001-0802-73af01ac0002&time=2014-08-21T15:59:00.000&width=1280&height=720
