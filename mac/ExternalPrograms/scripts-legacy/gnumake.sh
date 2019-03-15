# ------------------
#      gnumake
# ------------------
# $Id:  $
# Copyright (c) 2007, Ippei Ukai

# prepare

# export REPOSITORYDIR="/PATH2HUGIN/mac/ExternalPrograms/repository" \
# ARCHS="ppc i386" \
#  ppcTARGET="powerpc-apple-darwin8" \
#  i386TARGET="i386-apple-darwin8" \
#  ppcMACSDKDIR="/Developer/SDKs/MacOSX10.4u.sdk" \
#  i386MACSDKDIR="/Developer/SDKs/MacOSX10.4u.sdk" \
#  ppcONLYARG="-mcpu=G3 -mtune=G4" \
#  i386ONLYARG="-mfpmath=sse -msse2 -mtune=pentium-m -ftree-vectorize" \
#  ppc64ONLYARG="-mcpu=G5 -mtune=G5 -ftree-vectorize" \
#  OTHERARGs="";

# -------------------------------
# 20091206.0 sg Script tested and used to build 2009.4.0-RC3
# 20100624.0 hvdw More robust error checking on compilation
# -------------------------------

# init

fail()
{
        echo "** Failed at $1 **"
        exit 1
}


let NUMARCH="0"

mkdir -p "$REPOSITORYDIR/bin";
mkdir -p "$REPOSITORYDIR/lib";
mkdir -p "$REPOSITORYDIR/include";

#patch
make install_source
#cd ./make

# compile

# remove 64-bit archs from ARCHS
ARCHS_TMP=$ARCHS
ARCHS=""
for ARCH in $ARCHS_TMP
do
 if [ $ARCH = "i386" -o $ARCH = "i686" -o $ARCH = "ppc" -o $ARCH = "ppc750" -o $ARCH = "ppc7400" ]
 then
   NUMARCH=$(($NUMARCH + 1))
   if [ -n "$ARCHS" ]
   then
     ARCHS="$ARCHS $ARCH"
   else
     ARCHS=$ARCH
   fi
 fi
done

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
   ARCHARGs="$i386ONLYARG"
   OSVERSION="$i386OSVERSION"
   CC=$i386CC
   CXX=$i386CXX
 elif [ $ARCH = "ppc" -o $ARCH = "ppc750" -o $ARCH = "ppc7400" ] ; then
   TARGET=$ppcTARGET
   MACSDKDIR=$ppcMACSDKDIR
   ARCHARGs="$ppcONLYARG"
   OSVERSION="$ppcOSVERSION"
   CC=$ppcCC
   CXX=$ppcCXX
 fi

 env \
  CC=$CC CXX=$CXX \
  CFLAGS="-isysroot $MACSDKDIR -arch $ARCH $ARCHARGs $OTHERARGs -O3 -dead_strip" \
  CXXFLAGS="-isysroot $MACSDKDIR -arch $ARCH $ARCHARGs $OTHERARGs -O3 -dead_strip" \
  CPPFLAGS="-I$REPOSITORYDIR/include" \
  LDFLAGS="-L$REPOSITORYDIR/lib -mmacosx-version-min=$OSVERSION -dead_strip -prebind" \
  NEXT_ROOT="$MACSDKDIR" \
  ./configure --prefix="$REPOSITORYDIR" --disable-dependency-tracking \
    --host="$TARGET" --exec-prefix=$REPOSITORYDIR/arch/$ARCH \
   --with-libiconv-prefix=$REPOSITORYDIR --with-libintl-prefix=$REPOSITORYDIR \
   --program-transform-name='s/^make$/gnumake/' || fail "configure step for $ARCH";

 make clean;
 make || fail "failed at make step of $ARCH";
 make install || fail "make install step of $ARCH";

done


# merge execs

for program in bin/gnumake
do

	if [ $NUMARCH -eq 1 ] ; then
   if [ -f $REPOSITORYDIR/arch/$ARCHS/$program ] ; then
		 echo "Moving arch/$ARCHS/$program to $program"
  	 mv "$REPOSITORYDIR/arch/$ARCHS/$program" "$REPOSITORYDIR/$program";
  	 strip "$REPOSITORYDIR/$program";
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
 	strip "$REPOSITORYDIR/$program";

done

#cd ../
