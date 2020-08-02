# PostgreSQL Setup

In this tutorial, we'll use the IMDB dataset from the "How Good are Query Optimizers, Really?"[^howgood] paper. You can quickly setup a PostgreSQL database with this data [via a virtual machine](https://git.io/imdb), or you can download a [a PostgreSQL dump from the Harvard dataverse](https://dataverse.harvard.edu/dataset.xhtml?persistentId=doi:10.7910/DVN/2QYZBT) and manually load the data yourself.

Don't forget to configure PostgreSQL with sane defaults. The [VM setup](https://git.io/imdb) will do this automatically, but at a minimum you should set the `shared_buffers` variable to something larger than the default (around 25% to 40% of your total RAM [is recommended](https://wiki.postgresql.org/wiki/Tuning_Your_PostgreSQL_Server)). 

Assuming you've used the virtual machine, we can test that the DB was setup correctly:
```
# add -h localhost if you are connecting to the VM from your host machine
$ psql -U imdb 
psql (12.3)
Type "help" for help.

imdb=# select count(*) from title;
  count  
---------
 2528312
(1 row)
```

## Install the Bao extension

With our PostgreSQL database setup, it is time to install Bao. If you are using a virtual machine, the following steps should be performed from within the VM (i.e., type `vagrant ssh` to login).

```bash
$ git clone https://github.com/learnedsystems/BaoForPostgreSQL
$ cd BaoForPostgreSQL
$ ls
bao_server  branding  COPYING  LICENSE  pg_extension  
README.md  run_queries.py  sample_queries
```

The directory `pg_extension` contains the code for the PostgreSQL extension. We'll install that next, using the PGXS system. Make sure your machine has the these installed. 

For Ubuntu, you need these packages:
```bash
sudo apt-get install postgresql-server-dev-all postgresql-common
```

For Arch Linux:
```
sudo pacman -S postgresql-libs
```


With the correct packages installed, we can proceed to install the Bao extension. First, turn off PostgreSQL:

```bash
systemctl stop postgresql
```

Next, build and install the Bao PostgreSQL extension.

```bash
# depending on your setup, this may require sudo
cd pg_extension
make USE_PGXS=1 install
```

If everything goes correctly, you should see output like below:

```
$ make USE_PGXS=1 install
/usr/bin/clang -Wno-ignored-attributes -fno-strict-aliasing -fwrapv -O2  -I. -I./ -I/usr/include/postgresql/server -I/usr/include/postgresql/internal  -D_FORTIFY_SOURCE=2 -D_GNU_SOURCE -I/usr/include/libxml2  -flto=thin -emit-llvm -c -o main.bc main.c
gcc -Wall -Wmissing-prototypes -Wpointer-arith -Wdeclaration-after-statement -Werror=vla -Wendif-labels -Wmissing-format-attribute -Wformat-security -fno-strict-aliasing -fwrapv -fexcess-precision=standard -Wno-format-truncation -Wno-stringop-truncation -march=x86-64 -mtune=generic -O2 -pipe -fno-plt -fPIC -I. -I./ -I/usr/include/postgresql/server -I/usr/include/postgresql/internal  -D_FORTIFY_SOURCE=2 -D_GNU_SOURCE -I/usr/include/libxml2   -c -o main.o main.c
gcc -Wall -Wmissing-prototypes -Wpointer-arith -Wdeclaration-after-statement -Werror=vla -Wendif-labels -Wmissing-format-attribute -Wformat-security -fno-strict-aliasing -fwrapv -fexcess-precision=standard -Wno-format-truncation -Wno-stringop-truncation -march=x86-64 -mtune=generic -O2 -pipe -fno-plt -fPIC -shared -o pg_bao.so main.o  -L/usr/lib  -Wl,-O1,--sort-common,--as-needed,-z,relro,-z,now -L/usr/lib  -Wl,--as-needed  
/usr/bin/mkdir -p '/usr/lib/postgresql'
/usr/bin/mkdir -p '/usr/share/postgresql/extension'
/usr/bin/mkdir -p '/usr/share/postgresql/extension'
/usr/bin/install -c -m 755  pg_bao.so '/usr/lib/postgresql/pg_bao.so'
/usr/bin/install -c -m 644 .//pg_bao.control '/usr/share/postgresql/extension/'
/usr/bin/install -c -m 644 .//pg_bao--0.0.1.sql  '/usr/share/postgresql/extension/'
/usr/bin/mkdir -p '/usr/lib/postgresql/bitcode/pg_bao'
/usr/bin/mkdir -p '/usr/lib/postgresql/bitcode'/pg_bao/
/usr/bin/install -c -m 644 main.bc '/usr/lib/postgresql/bitcode'/pg_bao/./
cd '/usr/lib/postgresql/bitcode' && /usr/bin/llvm-lto -thinlto -thinlto-action=thinlink -o pg_bao.index.bc pg_bao/main.bc
```

Now that the Bao extension is installed, we have to tell PostgreSQL to load it. To do this, modify your `postgresql.conf` file to load the `pg_bao` shared library. If you are using the VM, you can do this like so (as a superuser):

```bash
echo "shared_preload_libraries = 'pg_bao'" >> /media/data/pg_data/data/postgresql.conf
```

Next, we restart PostgreSQL:
```bash
systemctl restart postgresql
```


We can now reconnect to the database and test to make sure Bao is installed:
```
# add -h localhost if you are connecting to the VM from your host machine
$ psql -U imdb 
psql (12.3)
Type "help" for help.

imdb=# SHOW enable_bao;
 enable_bao 
------------
 off
(1 row)

```

If the `enable_bao` is present (it defaults to `off`), then the Bao extension is installed. **If instead you see an error like below, then the Bao extension has not been installed:**

```
imdb=# show enable_bao;
ERROR:  unrecognized configuration parameter "enable_bao"
```

# Notes

[^howgood]: Leis, Viktor, Andrey Gubichev, Atanas Mirchev, Peter Boncz, Alfons Kemper, and Thomas Neumann. “How Good Are Query Optimizers, Really?” PVLDB, VLDB ’15, 9, no. 3 (2015): 204–215. https://doi.org/10.14778/2850583.2850594.
