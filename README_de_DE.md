<h1 align="center">
<img src="https://i.imgur.com/DDkfI9i.png" alt="Kubu" width="300"/>
<br/><br/>
Kubu Core [KUBU, Ᵽ]  
</h1>


Sprache ändern: [EN](./README.md) | [CN](./README_zh_CN.md) | [PT](./README_pt_BR.md) | [FA](./README_fa_IR.md) | [VI](./README_vi_VN.md) | [FR](./README_fr_FR.md) | [JA](./README_ja_JP.md) | DE | 



Kubu, erstellt von einem der ursprünglichen Dogecoin-Shibes, ist eine Community-fokussierte Kryptowährung, mit dem Ziel, eine neue und spaßige Community, ähnlich der ursprünglichen Dogecoin-Community, zu schaffen.

Im Gegensatz zu allen vorherigen Iterationen ist Kubu ein Layer-1 Coin. 
Das bedeutet, es gibt keine Liquiditätspools, keine Sperrung von Wallets und keine verwirrenden Smart Contracts. 
Kubu ist eine einfache, eigenständige Blockchain.

Die Kubu Core-Software ermöglicht es jedem, einen Node in den Kubu-Blockchain-Netzwerken zu betreiben und verwendet die Scrypt-Hashing-Methode für den "Proof of Work". 
Sie wurde aus den Programmen Dogecoin Core, Bitcoin Core und anderen Kryptowährungen angepasst.

Für Informationen zu den Standardgebühren im Kubu-Netzwerk lesen Sie die [Gebührenempfehlungen](doc/fee-recommendation_DE.md).

**Website:** [kubucoin.org](https://kubucoin.org)

## Unterschiede zu Dogecoin

Kubu ist eine Abspaltung von Dogecoin. Um die Vertrautheit zu wahren, werden wir versuchen, Kubu ähnlich wie Dogecoin zu behandeln.

Änderungen:

* Adressen beginnen mit `P` statt `D`
* BIPS-Funktionen übernommen
* AuxPow Chain ID 63 (Merged Mining aktiviert)
* UI im Kubu-Style



## Verwendung 💻

Um Ihre Reise mit Kubu Core zu beginnen, lesen Sie den [Quick Guide](doc/README_windows_DE.md), [Installationsanweisungen](INSTALL.md) und das [Einrichtungstutorial](doc/getting-started.md).

Die JSON-RPC-API von Kubu Core ist selbstdokumentierend und kann mit kubu-cli help durchsucht werden, während detaillierte Informationen zu jedem Befehl mit kubu-cli help <Befehl> angezeigt werden können. 
Alternativ lesen Sie die [Bitcoin Core Dokumentation](https://developer.bitcoin.org/reference/rpc/) - die ein ähnliches Protokoll implementiert - um eine durchsuchbare Version zu erhalten.

### Ports

Kubu Core verwendet standardmäßig den Port `33874` für die Peer-to-Peer-Kommunikation, 
die zum Synchronisieren der "mainnet"-Blockchain und zum Informieren über neue Transaktionen und Blöcke benötigt wird. 
Zusätzlich kann ein JSONRPC-Port geöffnet werden, der standardmäßig für Mainnet-Knoten auf Port 33873 eingestellt ist. 
Es wird dringend empfohlen, RPC-Ports nicht dem öffentlichen Internet preiszugeben.

| Function | mainnet | testnet | regtest |
| :------- | ------: | ------: | ------: |
| P2P      |   33874 |   44874 |   18444 |
| RPC      |   33873 |   44873 |   18332 |

## Fortlaufende Entwicklung 💻

Kubu Core ist eine Open-Source- und Community-getriebene Software. 
Der Entwicklungsprozess ist offen und öffentlich einsehbar; jeder kann die Software sehen, darüber diskutieren und daran arbeiten.


Hauptentwicklungsressourcen:

* [GitHub Projekte](https://github.com/kubucoindev/kubu/projects) werden verwendet,
 um geplante und laufende Arbeiten für bevorstehende Veröffentlichungen zu verfolgen.

* [GitHub Discussion](https://github.com/kubucoindev/kubu/discussions) wird genutzt, 
  um Features (geplante und ungeplante) zu diskutieren die mit der Entwicklung der Kubu Core-Software, den zugrunde liegenden Protokollen und dem KUBU-Vermögenswert zusammenhängen.

* [KubuDev subreddit](https://www.reddit.com/r/kubudev/)


### Versionsstrategie

Versionsnummern folgen dem Schema ```major.minor.patch```.

### Branches

Es gibt 3 Arten von Branches in diesem Repository:

- **master**: Stabil, enthält die neueste Version der letzten *major.minor* Veröffentlichung.
- **maintenance**: Stabil, enthält die neueste Version früherer Veröffentlichungen, die noch aktiv gewartet werden. Format: <version>-maint
- **development**: Instabil, enthält neuen Code für geplante Veröffentlichungen. Format: ```<version>-dev```

*Master- und Wartungs-Branches sind ausschließlich durch Veröffentlichungen änderbar.*
*Geplante Veröffentlichungen haben immer einen Entwicklungs-Branch, und Pull Requests sollten gegen diese eingereicht werden.*
*Wartungs-Branches sind **nur für Bugfixes gedacht,** reichen Sie bitte neue Funktionen gegen den Entwicklungszweig mit der höchsten Version ein.*

## Mitwirken 🤝

Wenn Sie einen Fehler finden oder Probleme mit dieser Software haben, melden Sie dies bitte über das [Report System](https://github.com/kubucoindev/kubu/issues/new?assignees=&labels=bug&template=bug_report.md&title=%5Bbug%5D+).

Bitte sehen Sie sich den [Beitrag zur Mitwirkung](CONTRIBUTING.md) an, um zu erfahren, wie Sie an der Entwicklung von Kubu Core teilnehmen können. 
Oft gibt es [Themen, bei denen Hilfe benötigt wird](https://github.com/kubucoindev/kubu/labels/help%20wanted), bei denen Ihre Beiträge einen großen Einfluss haben und sehr geschätzt werden.

## Communities 🐸

Sie können sich der Community in verschiedenen sozialen Medien anschließen, um Leute zu treffen, zu diskutieren, 
die neuesten Kubu-Memes zu finden, etwas über Kubu zu lernen oder um Ideen zu teilen.

Hier sind einige Links:

* [r/Kubu Reddit](https://www.reddit.com/r/kubu/) Kubu Reddit
* [Discord](https://kubucoin.org/discord) Offizieller Kubu Discord Server
* [Twitter/X](https://twitter.com/KubuNetwork)


## Häufig gestellte Fragen ❓

Haben Sie eine Frage zu Kubu? 
Eine Antwort befindet sich vielleicht bereits im [FAQ](doc/FAQ_DE.md) oder im [Frage-und-Antwort-Bereich](https://github.com/kubucoindev/kubu/discussions/categories/q-a) des Diskussionsforums!

## Lizenz ⚖️
Kubu Core wird unter den Bedingungen der MIT-Lizenz veröffentlicht. Siehe 
[COPYING](COPYING) für weitere Informationen oder besuchen Sie
[opensource.org](https://opensource.org/licenses/MIT)
