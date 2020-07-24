# roq-samples


A collection of samples meant to demonstrate some features of the Roq API.

Direct third-party dependencies

* [fmt](https://github.com/fmtlib/fmt) (MIT License)
* [gflags](https://github.com/gflags/gflags) (BDS 3-Clause License)


## Operating Systems

* Linux


## Prerequisites

The library is designed to be compatible with the conda package manager.

This is one way to create a conda environment and install the required
packages

```bash
wget -N https://repo.continuum.io/miniconda/Miniconda3-latest-Linux-x86_64.sh

bash Miniconda3-latest-Linux-x86_64.sh -b -u -p ~/miniconda3

source ~/miniconda3/bin/activate

conda install -y \
    git \
    cmake \
    gxx_linux-64 \
    gdb_linux-64

conda install -y --channel https://roq-trading.com/conda/stable \
    roq-api \
    roq-logging \
    roq-client
```


## Building

```bash
git submodule update --init --recursive

cmake \
    -DCMAKE_AR="$AR" \
    -DCMAKE_RANLIB="$RANLIB" \
    -DCMAKE_NM="$NM" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DBUILD_TESTING=ON \
    .

make -j4
```


## Using

* [Example 1](./src/roq/samples/example-1/README.md)
  * Connect to market gateway
  * Subscribe using regex patterns
* [Example 2](./src/roq/samples/example-2/README.md)
  * Manage disconnect
  * Process incremental market data update
  * Maintain a market depth view
  * Update a simple model
* [Example 3](./src/roq/samples/example-3/README.md)
  * Maintain positions
  * Place limit orders
  * Deal with order acks and updates
  * Historical simulation
  * Live trading


### Event Logs (install)

Simulation requires you to either capture your own event logs (automatically
done by the gateways) or use sample data

```bash
conda install -y --channel https://roq-trading.com/conda/stable \
    roq-data
```

Data can now be found in the `$CONDA_PREFIX/share/roq/data/` directory.


### Gateways (install, configure, run)


#### [Deribit](https://roq-trading.com/docs/gateways/deribit/index.html)

```bash
conda install -y --channel https://roq-trading.com/conda/stable \
    roq-deribit
```

It is easiest to start from a config file template

```bash
cp $CONDA_PREFIX/share/roq/deribit/config.toml ./deribit.toml
```

Edit this file and update with your Deribit API credentials
([link](https://test.deribit.com/main#/account?scrollTo=api)).

You should look for these lines and replace as appropriate

```text
login = "YOUR_DERIBIT_LOGIN_GOES_HERE"
secret = "YOUR_DERIBIT_SECRET_GOES_HERE"
```

Launch the gateway

```bash
roq-deribit \
    --name "deribit" \
    --config-file deribit.toml \
    ---client-listen-address ~/deribit.sock
```

#### [Coinbase Pro](https://roq-trading.com/docs/gateways/deribit/index.html)

```bash
conda install -y --channel https://roq-trading.com/conda/stable \
    roq-coinbase-pro
```

It is easiest to start from a config file template

```bash
cp $CONDA_PREFIX/share/roq/coinbase-pro/config.toml ./coinbase-pro.toml
```

Edit this file and update with your Coinbase Pro API credentials
([link](https://public.sandbox.pro.coinbase.com/profile/api)).

You should look for these lines and replace as appropriate

```text
login = "YOUR_COINBASE_PRO_API_KEY_GOES_HERE"
password = "YOUR_COINBASE_PRO_PASSPHRASE_GOES_HERE"
secret = "YOUR_COINBASE_PRO_SECRET_GOES_HERE"
```

Launch the gateway

```
roq-coinbase-pro \
    --name "coinbase-pro" \
    --config-file coinbase-pro.toml \
    ---client-listen-address ~/coinbase-pro.sock
```

## Links

* [Documentation](https://roq-trading.com/docs)
* [Contact us](mailto:info@roq-trading.com)
