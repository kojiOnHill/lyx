# This will match strings with Qt accelerators, like "so&me message",
# without having to specify the ampersand.
function git-grep-accel-robust ()
{
  # TODO preserve "git" args? i.e., allow user to not use -i, etc.
  #
  # Insert the string "&\?" after each character to allow for an ampersand
  # to be anywhere.
  # (https://unix.stackexchange.com/a/5981/197212)
  search_str="$( echo "$1" | sed 's/.\{1\}/&\&\\\?/g')"
  git grep -i "${search_str}"
}


function update-alternatives-add-new-gcc-and-clang ()
{

  # It is in theory fine to run this multiple times. i.e., shouldn't affect
  # anything if it's already installed.

  # todo: should we clear previous ones?
  #
  # These will give errors (and exit) the first time, i.e., when there are no
  # alternatives set:
  #
  #   update-alternatives: error: no alternatives for clang++
  #
  #sudo update-alternatives --remove-all gcc
  #sudo update-alternatives --remove-all g++
  #sudo update-alternatives --remove-all gfortran
  #
  #sudo update-alternatives --remove-all clang
  #sudo update-alternatives --remove-all clang++

  # Order is important. The last one will be the default.
  gcc_versions=( $(ls /usr/bin/gcc-* | grep -o -P '(?<=gcc-)\d+(?=$)') )
  for v in "${gcc_versions[@]}"; do
    echo "${v}"
    sudo update-alternatives --install "/usr/bin/gcc" "gcc" "/usr/bin/gcc-${v}" "${v}"
    sudo update-alternatives --install "/usr/bin/g++" "g++" "/usr/bin/g++-${v}" "${v}"
    sudo update-alternatives --install "/usr/bin/gfortran" "gfortran" "/usr/bin/gfortran-${v}" "${v}"
  done

  clang_versions=( $(ls /usr/bin/clang-* | grep -o -P '(?<=clang-)\d+(?=$)') )
  for v in "${clang_versions[@]}"; do
    echo "${v}"
    sudo update-alternatives --install "/usr/bin/clang" "clang" "/usr/bin/clang-${v}" "${v}"
    sudo update-alternatives --install "/usr/bin/clang++" "clang++" "/usr/bin/clang++-${v}" "${v}"
  done

  echo -e "now you can use 'switch-gcc-to' and 'switch-clang-to' (from lyx-tester switches)"
}
