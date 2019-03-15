#!/usr/bin/env bash

# This script attempts to copy needed 3rd party libraries into the application
# bundle. It will set the 'install_name' for each library so that it references
# Library directory. The Script will change every library it can find. 
# Each of these libraries needs to have an absolute install path so we can
# copy it (already taken care of if using the ones built in ExternalPrograms).

BASE_DIR="$2"
cd ${BASE_DIR}

CMAKE_SOURCE_DIR="$3"

APPLICATION_NAME="$1"
APPLICATION_APP_BUNDLE="${APPLICATION_NAME}.app"
  APPLICATION_BINDIR="${APPLICATION_APP_BUNDLE}/Contents/MacOS"
APPLICATION_APP_NAME="${APPLICATION_BINDIR}/${APPLICATION_NAME}"
   PLUGINS_PATH="${APPLICATION_APP_BUNDLE}/Contents/Libraries"
RPATH_LIBRARY_PATH="@executable_path/../Libraries"


echo "*-----------------------------------------------------------*"
echo "* Copying Support Libraries for ${APPLICATION_APP_BUNDLE}"
echo "* Located in ${BASE_DIR}"

mkdir -p "${PLUGINS_PATH}"

get_libraries() {
  local LIBRARIES=$(echo $(otool -L $1 | grep -v ${RPATH_LIBRARY_PATH} - | grep -v \/System\/Library - | grep -v \/usr\/lib - | sed -ne '1!p' | sed -e 's/(.*)//' | sort -u))
  if [ -n "$LIBRARIES" ]; then
    for library in $LIBRARIES
    do
      update_library $library $1
    done
  fi
  install_name_tool -delete_rpath "${CMAKE_SOURCE_DIR}/mac/ExternalPrograms/repository/lib" $1 2&>/dev/null
}

resolve_symlink(){
  local lib=$1
  while [ -L "${lib}" ]; do
    lib="$(dirname ${lib})/$(readlink ${lib})"
  done
  echo "${lib}"
}

update_library() {
  local lib="$1"
  local bin="$2"
  
  local real_lib=$(resolve_symlink ${lib})
  local real_lib_file=$(basename ${real_lib})
  if [ ! -f "${BASE_DIR}/${PLUGINS_PATH}/${real_lib_file}" ]; then 
    echo "* Installing Library -->$1<-- into ${APPLICATION_APP_BUNDLE} " 
    cp "${real_lib}" "${BASE_DIR}/${PLUGINS_PATH}" || exit 1
    install_name_tool -id "${RPATH_LIBRARY_PATH}/${real_lib_file}" "${BASE_DIR}/${PLUGINS_PATH}/${real_lib_file}"
    chmod 755 "${BASE_DIR}/${PLUGINS_PATH}/${real_lib_file}"
    get_libraries "${BASE_DIR}/${PLUGINS_PATH}/${real_lib_file}"
  fi
  install_name_tool -change "${lib}" "${RPATH_LIBRARY_PATH}/${real_lib_file}" "${bin}"
}


# -----------------------------------------------------------------------------
# Copy libraries for all exetuables in APPLICATION_BINDIR
# -----------------------------------------------------------------------------

for _exe in ${BASE_DIR}/${APPLICATION_BINDIR}/*; do
  get_libraries $_exe
done


# -----------------------------------------------------------------------------
# Copy ExifTool into the stitchers
# -----------------------------------------------------------------------------
if [ "${APPLICATION_APP_BUNDLE}" == "PTBatcherGUI.app" ] || [ "${APPLICATION_APP_BUNDLE}" == "HuginStitchProject.app" ]; then

  if [ ! -x "${APPLICATION_APP_BUNDLE}/Contents/Resources/ExifTool/exiftool" ]; then
    echo "* Installing ExifTool into ${APPLICATION_APP_BUNDLE} " 
    mkdir -p "${APPLICATION_APP_BUNDLE}/Contents/Resources/ExifTool"
    cp -v "${CMAKE_SOURCE_DIR}/mac/ExternalPrograms/repository/"Image-ExifTool-*/exiftool  "${APPLICATION_APP_BUNDLE}/Contents/Resources/ExifTool"
    cp -r "${CMAKE_SOURCE_DIR}/mac/ExternalPrograms/repository/"Image-ExifTool-*/lib       "${APPLICATION_APP_BUNDLE}/Contents/Resources/ExifTool"
  fi

fi

