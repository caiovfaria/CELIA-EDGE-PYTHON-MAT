# Registro de uso de IA · C.E.L.I.A.

A FIAP permite o uso de IA desde que os prompts sejam registrados. Este arquivo lista cada pedido feito às IAs na parte de Sensor (Wokwi) e IA de buracos: data, ferramenta, pedido (resumido ou literal) e o que foi aproveitado.

Toda a saída das IAs foi revisada pela equipe antes de entrar no projeto.

| Data | IA | Pedido | Resultado aproveitado |
|---|---|---|---|
| 11/11/2026 | Claude | Ler os novos documentos da equipe (Documentação do Módulo Edge e Requisitos e Backlog) e entender o projeto | Resumo do projeto e lista de ajustes: `retain` do MQTT, tempo de demonstração, faixa de calor de 40–41 °C, ODS, ligação com Python, HTTPS e falso positivo do YOLO |
| 11/11/2026 | Claude | "Meu trabalho no grupo é montar o sensor no Wokwi e treinar a IA para identificar os buracos. Como você conseguiria me ajudar?" | Divisão de tarefas entre aluno, Claude e Codex |
| 11/11/2026 | Codex | Preparar o contexto de colaboração entre as duas IAs | Documento de contexto enviado ao Claude |
| 11/11/2026 | Claude | Avaliar o contexto preparado pelo Codex e sugerir o melhor caminho | Formato "um faz, o outro revisa", ordem por nota e um arquivo de decisões (`AGENTS.md`) |
| 11/11/2026 | Claude | "Faça o prompt para ensinar o Codex o que vamos fazer" | Prompt com etapas, decisões e lista de revisão para o Codex |
| 11/11/2026 | Claude | "Me manda um PDF explicando cada etapa de como vamos prosseguir" | `CELIA_Plano_de_trabalho.pdf` (plano de 11 a 15/11) |
| 11/11/2026 | Claude | "Comece a etapa 1" | `edge/` (circuito, código e lógica), `tests/teste_logica.cpp`, roteiro de demonstração, `AGENTS.md` e este arquivo. Testado no computador (70 verificações) e no Wokwi |
| 11/11/2026 | Claude | "Quero que teste se a etapa 1 foi concluída" | Conferência dos arquivos (hash), 70 verificações rodadas no computador da equipe, novo teste no Wokwi com tempos normais e prints em `edge/evidencias/teste_final/` |
| 11/11/2026 | Claude | "Termine a etapa 1 e depois me dê o prompt para mandar para o Codex" | Resumo final da etapa 1 e prompt de revisão para o Codex |
| 11/11/2026 | Codex | Revisão independente da Etapa 1 (prompt de revisão preparado pelo Claude) | 5 apontamentos médios: falha do HC-SR04 reaproveitava a última distância, partida sem leitura, conversão do eco, teste do retorno a zero em 64 bits, falta de registro dos passos 4–7 com 15 s |
| 12/11/2026 | Claude | Corrigir os apontamentos do Codex | Confirmação cancelada quando falta eco, tela SEM DADOS na partida, aviso de falha no OLED, conversão com 340 m/s e arredondamento, tempos em `uint32_t`, 20 testes novos de lógica e o teste do sketch com sensores falsos |
| 12/11/2026 | Codex | Revisões 2 e 3 da Etapa 1 | Pediu regra explícita nas bordas da calçada, tela SEM DADOS com a tentativa atual, teste do DHT22 com calor em atenção e tolerância do eco limitada ao observado |
| 12/11/2026 | Claude | Corrigir as revisões 2 e 3 | Regra em cm inteiros arredondando para baixo; calibração do HC-SR04 no Wokwi (28 ecos, 58,8 µs/cm, folga de 0,08 cm); SEM DADOS com a tentativa atual; teste do sketch com 16 verificações |
| 12/11/2026 | Codex | Revisão 4 (final) da Etapa 1 | "Etapa 1 aprovada para entrega". Um apontamento leve sobre a contagem de ecos (29 × 28) foi conferido: tabela e teste têm os mesmos 28 ecos |
| 12/11/2026 | Claude | "Meu Codex é ligado ao GitHub, faça um prompt" | Prompt para o Codex enviar a pasta da Etapa 1 ao repositório da equipe, com lista de arquivos, cuidados e conferência por hash |
| 12/11/2026 | Codex | Enviar a pasta da Etapa 1 ao repositório da equipe | Commit na main com 35 arquivos da Etapa 1 e o .gitignore (36 ao todo). |

<!-- Acrescente abaixo os próximos prompts, inclusive os feitos ao Codex. -->
