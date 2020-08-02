# Bao Server Configuration Variables

The Bao sever is configured through the `bao.cfg` file in the `bao_server` directory. The default configuration file, reproduced below, contains a description of each variable.

```ini
[bao]
# ==============================================================
# BAO SERVER SETTINGS
# ==============================================================

# port to listen on. Note that the corresponding PostgreSQL
# variable, bao_port, must be set to match.
Port = 9381

# network address to listen on. If not localhost, don't forget
# to set the PostgreSQL bao_host variable.
ListenOn = localhost

# ==============================================================
# EXPLORATION MODE SETTINGS
# ==============================================================

# maximum time a query should reasonably take (used in
# exploration mode).
MaxQueryTimeSeconds = 120

# psycopg2 / JDBC connection string to access PostgreSQL
# (used by the experiment runner to prevent regressions)
PostgreSQLConnectString = user=imdb
```
