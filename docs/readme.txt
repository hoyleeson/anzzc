automake project build step:
#create project
1. autoscan   
2. mv configure.scan configure.ac
3. edit configure.ac
4. aclocal
5. autoconf
6. autoheader
7. touch ChangeLog AUTHORS NEWS README 
8. libtoolize --automake
9. Makefile.am 	(important)

#create make & build
9. automake --add-missing
10../configure --prefix=$PWD/_install
11.make
12.make install
13.make dist
14.make clean
15.make distclean



