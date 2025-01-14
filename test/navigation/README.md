# Test files for Navigation
## Synopsis
These folders contain several files that allow to test the navigation.

The folder ./script contains the ones for the GlibDBus version:
test-poi.py
test-location-input.py
test-address-input.py 
test-map-viewer-control.py
test-guidance.py
test-route-calculation.py
test-all

The folder ./script-capi contains the ones for the CommonAPI version
NB: For the time being, only the GlibDBus version of navigation runs well

The folder ./resource contains the resource files (e.g. the address) used as input by the script

The folder ./log contains the generated sequence charts (see below)  

There's a optional mechanism to trig with DLT, please build the stuff in dlt-triggers before:

##How to build the trigger
Under ./dlt-triggers
```
mkdir build
cd build
cmake ../
make 
make install
```
##Tested targets
Desktop: Tested under Ubuntu 16.04 LTS 64 bits
##How-to test 
First, open a new terminal and launch the navigation and the poi server by entering:
```
./run -r -p
```
Unitary tests:
(under ./script folder)
```
./test-poi.py -l ../resource/location.xml
./test-location-input.py -l locations.xml
./test-route-calculation.py -r routes.xml
./test-address-input.py -l location.xml
./test-guidance.py -r route.xml
./test-map-viewer-control.py
```
To launch all the tests:
```
./test-all
```
NB: The locations and the routes are defined for the map database around Geneva (the reference map database into the repository). 

##Sequence charts
In order to illustrate the DBus exchanges, some sequence diagrams are available:
location-input.pdf -->  ./test-location-input.py -l location.xml
route-calculation.pdf --> ./test-route-calculation.py -r route.xml
guidance.pdf --> ./test-guidance.py -r route.xml (partial)
map-viewer-control.pdf --> ./test-map-viewer-control.py

NB: The sequence charts have been caught and generated with an Elektrobit tool (EBSolys)

