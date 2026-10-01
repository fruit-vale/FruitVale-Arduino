# Como publicar no Gerenciador de Bibliotecas do Arduino

1. **Crie o repositório público** no GitHub no endereço do campo `url` do
   `library.properties` (hoje `https://github.com/fruit-vale/FruitVale-Arduino`).
   Se usar outro endereço, ajuste o `url` antes de continuar.

2. **Envie o código:**
   ```bash
   cd FruitVale-Arduino
   git init -b main
   git add .
   git commit -m "FruitVale 1.0.0"
   git remote add origin https://github.com/fruit-vale/FruitVale-Arduino.git
   git push -u origin main
   ```

3. **Valide** (o mesmo teste que o registro faz):
   ```bash
   arduino-lint --library-manager submit --compliance strict "$PWD"
   ```
   Deve terminar com `0 ERRORS`.

4. **Crie a release** com a mesma versão do `library.properties`:
   ```bash
   git tag 1.0.0
   git push origin 1.0.0
   ```

5. **Registre a biblioteca (uma vez só):** no repositório
   https://github.com/arduino/library-registry, edite `repositories.txt`
   (botão ✏️), acrescente a linha
   `https://github.com/fruit-vale/FruitVale-Arduino` e abra o pull request.
   Um robô valida em minutos; depois do merge, a biblioteca aparece no
   Gerenciador da IDE em até ~1 dia.

## Novas versões

Não precisa de novo pull request: aumente `version=` no `library.properties`
(ex.: `1.0.1`), faça commit e crie a tag igual (`git tag 1.0.1 && git push origin 1.0.1`).
O registro detecta a tag sozinho.

Atenção: uma versão publicada não pode ser apagada do registro, e o nome
`FruitVale` fica reservado para este repositório.
