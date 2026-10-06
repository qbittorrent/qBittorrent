# Swarm Discovery - Proposta de Implementação

## Contexto
Issue #11176 solicita reconhecimento de torrents que baixam o mesmo arquivo/peça e compartilhamento de progresso entre eles, similar ao recurso de swarm discovery do Vuze.

## Escopo Atual
A implementação completa requer alterações profundas em:
- Gerenciamento de peças e hash verification
- Camada de disco e escrita segura
- TorrentHandle e BitTorrentSession
- UI para expor torrents compartilhados

Este PR introduz apenas a infraestrutura base para descoberta de peças compartilhadas por hash.

## O que foi adicionado
- `src/base/bittorrent/swarmdiscovery.h/cpp`: `SwarmDiscoveryManager` singleton
  - Registro de torrents por conjunto de hashes de peças
  - Mapeamento peça -> torrents
  - Consultas de torrents/peças compartilhadas
  - Flag de habilitação

## Próximos passos necessários
1. Integrar registro no `Torrent` ao iniciar/pausar/remover
2. Calcular e armazenar hashes de peças de forma consistente entre torrents
3. Modificar `PieceManager` para reutilizar dados já presentes em disco de outro torrent
4. Garantir segurança de escrita e verificação de integridade
5. Adicionar opção em OptionsDialog para habilitar swarm discovery
6. Testes de integração com múltiplos torrents contendo peças idênticas

Esta base permite evoluir a feature sem quebrar o código existente.
