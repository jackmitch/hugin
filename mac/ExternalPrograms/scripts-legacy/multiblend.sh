# ------------------
# multiblend 0.31beta   
# ------------------
# $Id: multiblend.sh 1908 2007-02-05 14:59:45Z ippei $
# Copyright (c) 2007, Ippei Ukai
# 2011, Harry van der Wolf

# prepare

# export REPOSITORYDIR="/PATH2HUGIN/mac/ExternalPrograms/repository" \
# ARCHS="ppc i386" \
#  ppcTARGET="powerpc-apple-darwin7" \
#  i386TARGET="i386-apple-darwin8" \
#  ppcMACSDKDIR="/Developer/SDKs/MacOSX10.3.9.sdk" \
#  i386MACSDKDIR="/Developer/SDKs/MacOSX10.4u.sdk" \
#  ppcONLYARG="-mcpu=G3 -mtune=G4" \
#  i386ONLYARG="-mfpmath=sse -msse2 -mtune=pentium-m -ftree-vectorize" \
#  ppc64ONLYARG="-mcpu=G5 -mtune=G5 -ftree-vectorize" \
#  OTHERARGs="";

# -------------------------------
# 20111231.0 hvdw first version. Still experimental. Don't know which
#		  cross arch settings are necessary and which not.
# 20120106.0 hvdw adapt for version 0.2
# 20120426.0 hvdw adapt for 0.31 and compile x86_64 with gcc-4.6 for lion and up openmp compatibility
# -------------------------------

# init

fail()
{
        echo "** Failed at $1 **"
        exit 1
}

ORGPATH=$PATH

let NUMARCH="0"


for i in $ARCHS
do
  NUMARCH=$(($NUMARCH + 1))
done

mkdir -p "$REPOSITORYDIR/bin";
mkdir -p "$REPOSITORYDIR/lib";
mkdir -p "$REPOSITORYDIR/include";

# compile

for ARCH in $ARCHS
do

 mkdir -p "$REPOSITORYDIR/arch/$ARCH/bin";
 mkdir -p "$REPOSITORYDIR/arch/$ARCH/lib";
 mkdir -p "$REPOSITORYDIR/arch/$ARCH/include";

 ARCHARGs=""
 MACSDKDIR=""

 if [ $ARCH = "i386" -o $ARCH = "i686" ] ; then
   TARGET=$i386TARGET
   MACSDKDIR=$i386MACSDKDIR
#   ARCHARGs="$i386ONLYARG"
   # multiblend not yet fully compliant with openmp on 32bits
   ARCHARGs="-march=prescott -mtune=pentium-m -ftree-vectorize -mmacosx-version-min=10.5"
   OSVERSION="$i386OSVERSION"
   CC=$i386CC
   CXX=$i386CXX
   myPATH=$ORGPATH
   ARCHFLAG="-m32"
 elif [ $ARCH = "x86_64" ] ; then
   TARGET=$x64TARGET
   MACSDKDIR=$x64MACSDKDIR
   ARCHARGs="$x64ONLYARG"
   OSVERSION="$x64OSVERSION"
   CC="gcc-4.6"
   CXX="g++-4.6"
   ARCHFLAG="-m64"
   myPATH=/usr/local/bin:$PATH
 fi

 echo "## Now compiling $ARCH version ##\n"
 env \
   PATH=$myPATH \
   CC=$CC CXX=$CXX \
   CFLAGS="-isysroot $MACSDKDIR -I. -I$REPOSITORYDIR/include -O2 $ARCHFLAG $ARCHARGs $OTHERARGs -dead_strip" \
   CXXFLAGS="-isysroot $MACSDKDIR -I. -I$REPOSITORYDIR/include -O2 $ARCHFLAG $ARCHARGs $OTHERARGs -dead_strip" \
   CPPFLAGS="-I. -I$REPOSITORYDIR/include -I/usr/include" \
   LDFLAGS="-ltiff -ljpeg -lpng -L$REPOSITORYDIR/lib -L/usr/lib -mmacosx-version-min=$OSVERSION -dead_strip $ARCHFLAG" \
   NEXT_ROOT="$MACSDKDIR" \
   PKG_CONFIG_PATH="$REPOSITORYDIR/lib/pkgconfig" \
   $CXX -L$REPOSITORYDIR/lib/ -O2 -I$REPOSITORYDIR/include/ -I$MACSDKDIR/usr/include -isysroot $MACSDKDIR $ARCHFLAG $ARCHARGs $OTHERARGs \
   -ltiff -ljpeg -lpng multiblend.cpp -o multiblend || fail "compile step for $ARCH";

   mv multiblend $REPOSITORYDIR/arch/$ARCH/bin 
done


# merge execs

for program in bin/multiblend
do

 if [ $NUMARCH -eq 1 ] ; then
   if [ -f $REPOSITORYDIR/arch/$ARCHS/$program ] ; then
		 echo "Moving arch/$ARCHS/$program to $program"
  	 mv "$REPOSITORYDIR/arch/$ARCHS/$program" "$REPOSITORYDIR/$program";
  	 strip -x "$REPOSITORYDIR/$program";
  	 continue
	 else
		 echo "Program arch/$ARCHS/$program not found. Aborting build";
		 exit 1;
	 fi
 fi

 LIPOARGs=""

 for ARCH in $ARCHS
 do
 	if [ -f $REPOSITORYDIR/arch/$ARCH/$program ] ; then
		echo "Adding arch/$ARCH/$program to bundle"
 		LIPOARGs="$LIPOARGs $REPOSITORYDIR/arch/$ARCH/$program"
	else
		echo "File arch/$ARCH/$program was not found. Aborting build";
		exit 1;
	fi
 done

 lipo $LIPOARGs -create -output "$REPOSITORYDIR/$program";
 strip -x "$REPOSITORYDIR/$program";

done
