# NS3 MANET simulation

### Run Dockerfile

The Dockerfile copies all files from scratch folder.

`docker build . -t my-tag`

`docker run --rm -it my-tag bash`

## Example

- `./waf --run scenario1`

## To add visualization

`xhost +`

`docker run --rm -it --name name -e DISPLAY=$DISPLAY -v /tmp/.X11-unix:/tmp/.X11-unix:ro my-tag`

## Example

- `./waf --run scenario1 --vis`
