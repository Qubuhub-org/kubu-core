<h1 align="center">
<img src="https://i.imgur.com/DDkfI9i.png" alt="Kubu" width="300"/>
<br/><br/>
Kubu Core [KUBU, Ᵽ]
</h1>

Selecionar idioma: [EN](./README.md) | [CN](./README_zh_CN.md) | PT | [FA](./README_fa_IR.md) | [VI](./README_vi_VN.md) | [FR](./README_fr_FR.md) | [JA](./README_ja_JP.md) | [DE](./README_de_DE.md)

Kubu é uma criptomoeda focada na comunidade, criada por um dos shibes originais do Dogecoin de 2013. Foi criada com um propósito, criar uma comunidade nova e divertida, assim como a comunidade original do Dogecoin.

Diferente de todas as iterações anteriores, o Kubu é uma moeda de camada 1. Isso significa que não há piscinas de liquidez para drenar, carteiras em lista negra ou contratos inteligentes confusos. O Kubu é uma blockchain simples.

Assim como o Dogecoin, o software Kubu Core permite que qualquer pessoa opere um nó nas redes de blockchain do Kubu e usa o método de hash Scrypt para o Proof of Work. Ele é adaptado do Bitcoin Core e de outras criptomoedas.

Para informações sobre as taxas padrão usadas na rede Kubu, por favor consulte a [recomendação de taxas](doc/fee-recommendation.md).

**Website:** [kubucoin.org](https://kubucoin.org)

## Diferenças do Dogecoin

O Kubu é um fork do Dogecoin. Para fins de familiaridade, tentaremos manter o Kubu semelhante ao Dogecoin.

Mudanças:

* Endereços começam com `P` em vez de `D`
* Recursos BIPS começam no bloco gênese
* GUI temático para o Kubu

## Uso 💻

Para iniciar sua jornada com o Kubu Core, veja o [guia de instalação](INSTALL.md) e o tutorial [começando](doc/getting-started.md).

A API JSON-RPC fornecida pelo Kubu Core é autoexplicativa e pode ser navegada com `kubu-cli help`, enquanto informações detalhadas para cada comando podem ser visualizadas com `kubu-cli help <comando>`. Alternativamente, veja a [documentação do Bitcoin Core](https://developer.bitcoin.org/reference/rpc/) - que implementa um protocolo semelhante - para obter uma versão navegável.

### Portas

O Kubu Core, por padrão, usa a porta `33874` para comunicação peer-to-peer que é necessária para sincronizar a blockchain "mainnet" e se manter informado sobre novas transações e blocos. Adicionalmente, uma porta JSONRPC pode ser aberta, que por padrão é a porta `33873` para nós mainnet. É altamente recomendável não expor as portas RPC na internet pública.

| Função | mainnet | testnet | regtest |
| :------- | ------: | ------: | ------: |
| P2P      |   33874 |   44874 |   18444 |
| RPC      |   33873 |   44873 |   18332 |

## Desenvolvimento contínuo 💻

O Kubu Core é um software de código aberto e orientado pela comunidade. O processo de desenvolvimento é aberto e publicamente visível; qualquer um pode ver, discutir e trabalhar no software.

Principais recursos de desenvolvimento:

* [Projetos do GitHub](https://github.com/kubucoindev/kubu/projects) são usados para seguir o trabalho planejado e em progresso para futuros lançamentos.
* [Discussão do GitHub](https://github.com/kubucoindev/kubu/discussions) é usada para discutir recursos, planejados e não planejados, relacionados tanto ao desenvolvimento do software Kubu Core, quanto aos protocolos subjacentes e o ativo KUBU.
* [Subreddit KubuDev](https://www.reddit.com/r/kubudev/)

### Estratégia de versão
Os números de versão seguem a semântica ```principal.menor.correção```.

### Branches
Existem 3 tipos de branches neste repositório:

- **master:** Estável, contém a versão mais recente do último lançamento *principal.menor*.
- **manutenção:** Estável, contém a versão mais recente de lançamentos anteriores, que ainda estão sob manutenção ativa. Formato: ```<versão>-manut```
- **desenvolvimento:** Instável, contém novo código para lançamentos planejados. Formato: ```<versão>-dev```

*Os branches master e de manutenção são exclusivamente mutáveis por lançamento. Lançamentos planejados sempre terão um branch de desenvolvimento e pull requests devem ser submetidos contra esses. Branches de manutenção são apenas para **correções de bugs,*** por favor, submeta novos recursos contra o branch de desenvolvimento com a versão mais alta.

## Contribuindo 🤝

Se você encontrar um bug ou tiver problemas com este software, por favor, relate usando o [sistema de problemas](https://github.com/kubucoindev/kubu/issues/new?assignees=&labels=bug&template=bug_report.md&title=%5Bbug%5D+).

Por favor, veja [o guia de contribuição](CONTRIBUTING.md) para saber como você pode participar do desenvolvimento do Kubu Core. Muitas vezes há [tópicos buscando ajuda](https://github.com/kubucoindev/kubu/labels/help%20wanted) onde suas contribuições terão alto impacto e serão muito apreciadas.

## Comunidades 🐸

Você pode se juntar às comunidades em diferentes mídias sociais.
Para ver o que está acontecendo, conhecer pessoas e discutir, encontrar o último meme, aprender sobre o Kubu, dar ou pedir ajuda, para compartilhar seu projeto.

Aqui estão alguns lugares para visitar:

* [r/Kubu](https://www.reddit.com/r/kubu/)
* [Discord](https://kubucoin.org/discord)
* [Twitter/X](https://twitter.com/KubuNetwork)

## Perguntas Frequentes ❓

Você tem uma pergunta sobre o Kubu? A resposta pode estar na [FAQ](doc/FAQ.md) ou na [seção de Perguntas e Respostas](https://github.com/kubucoindev/kubu/discussions/categories/q-a) do quadro de discussão!

## Licença ⚖️
O Kubu Core é lançado sob os termos da licença MIT. Veja
[COPYING](COPYING) para mais informações ou veja
[opensource.org](https://opensource.org/licenses/MIT)
