This is a fork of the [Polipo caching web proxy](http://www.pps.jussieu.fr/~jch/software/polipo/). You can read the [original README here](https://github.com/doublec/namecoin-polipo/blob/master/README.original).

This fork adds support for looking up domain names using [Namecoin](http://www.bitcoin.org/smf/index.php?topic=6017), the distributed P2P naming system based on Bitcoin.

It needs libjansson for JSON parsing and libcurl for talking to the namecoind JSON-RPC server.

Once compiled, you can run it passing it the details about the namecoind server:

    $ git clone https://github.com/doublec/namecoin-polipo
    $ cd namecoin-polipo
    $ make
    $ ./polipo namecoindServer="127.0.0.1:8332" namecoindUsername=rpcuser namecoindPassword=rpcpassword

This will start the proxy, and it'll connect to the namecoind JSON-RPC server with the given details. It runs 'name_scan' to get the list of names. Any program using this proxy will have all .bit domains resolved by namecoind. Currently it only supports mapping to IP addresses. So namecoind values like the following work: {"map":{"":"192.0.32.10"}}. As an example 'bluishcoder.bit' hopefully resolves to the IP address of my server with my weblog using this version of polipo. It rescans namecoind when a request is made for a domain lookup and the last scan was more than 10 minutes ago. 

To use it with Firefox (or other browsers) go into the network settings and set the 'http proxy' to localhost, port 8123.

I decided to do this to avoid needing to run the ncproxy script that comes with namecoin since I don't need socks support. Comments, suggestions, patches are welcome.

Original README follows:
--------------------8<---------------------
Polipo README -*-text-*-
*************

Polipo is single-threaded, non blocking caching web proxy that has
very modest resource needs.  See the file INSTALL for installation
instructions.  See the texinfo manual (available as HTML after
installation) for more information.

Current information about Polipo can be found on the Polipo web page,

    http://www.pps.jussieu.fr/~jch/software/polipo/

I can be reached at the e-mail address below, or on the Polipo-users
mailing list:

    <polipo-users@lists.sourceforge.net>

Please see the Polipo web page for subscription information.

                                        Juliusz Chroboczek
                                        <jch@pps.jussieu.fr>
