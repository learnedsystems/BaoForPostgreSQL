import configparser

def read_config():
    config = configparser.ConfigParser()
    config.read("bao.cfg")

    if "bao" not in config:
        print("bao.cfg does not have a [bao] section.")
        exit(-1)

    config = config["bao"]
    return config
