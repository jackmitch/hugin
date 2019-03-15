External Programs
(This is for 10.6 and above and will built a 64bit only bundle and tools.)


HOWTO:
1. Download the source package of the external programs. You typically want to place it in the "ExternalPrograms" folder.
2. Edit ExternalPrograms/scripts/SetEnv-*.txt.org file appropriately, especially the myREPOSITORYDIR variable and
   save it as SetEnv-*.txt.
3. Open a Terminal window (bash is preferred).
5. 'cd' into the directory of source you want to compile. (eg. 'cd ExternalPrograms/jpeg-6b')
4. Set the variables for the compilation. (eg. 'source ../scripts/SetEnv-universal.txt')
6. Using the appropriate shell script, build the source. (eg. 'sh ../scripts/libjpeg.sh')


RESULT:
The programs and libraries will be installed into $myREPOSITORYDIR, which you can manage independently from the systems you are currently using (e.g /usr, /usr/local, /opt, /sw).


USAGE:
When compiling programs from source specifying the above directory as prefix. You probably have to specify the correct SDK (-isysroot) and MacOS target version (-mmacosx-version-min) as well.
You can make multiple 'repositories' as well. For example, you can make one for statically and one for dynamically linked product, or ones with different target architecture/OS versions. 


PROGRAMS:
The following sets are recommended and assumed for building Hugin. Different versions may require editing the version numbers in the scripts. 

[Dynamic Build for distributable Hugin.app and PTBatcherGUI.app]
boost (1.41)
libexpat (2.0.1)
libiconv
gettext (0.17)
libjpeg (8)
libpng (1.2.41)
libtiff (3.8.2)
ilmbase (1.0.1)
openexr16 (1.6.2)
pano13 (2.9.17)
wxmac28 (2.9.x)
libexiv2 (0.19)
lcms (1.17)
libxmi (1.2)
libglew (1.5.1)
gnumake (Darwin 9/gnumake-119/make)
enblend (4.0)

LICENSE:
The scripts for compiling universal builds are originally copyrighted by Ippei Ukai (2007-2008), and distributed under the modified BSD license.


$Id: howto.txt 1902 2007-02-04 22:27:47Z ippei 
