#include "ndr.h"

/**
 * Converte uma string inteira para letras maiúsculas.
 * Útil para normalizar nomes de variáveis e instruções.
 * dst: buffer de destino / src: string original
 */
void str_to_upper(char *dst, const char *src) {
    int i = 0;
    // Percorre a string de origem até o caractere nulo final
    while (src[i]) { 
        // Converte cada caractere individualmente
        dst[i] = (char)toupper((unsigned char)src[i]); 
        i++; 
    }
    // Adiciona o terminador nulo no final da string de destino
    dst[i] = '\0';
}

/**
 * Remove espaços em branco (espaços, tabs, \n) do início e do fim da string.
 * s: a string que será modificada "in-place".
 */
void str_trim(char *s) {
    /* --- LTRIM (Lado Esquerdo / Início) --- */
    int i = 0;
    // Conta quantos espaços existem no começo
    while (s[i] && isspace((unsigned char)s[i])) i++;
    
    // Se encontrou espaços, move o conteúdo útil para o início do buffer
    if (i > 0) memmove(s, s + i, strlen(s) - i + 1);

    /* --- RTRIM (Lado Direito / Fim) --- */
    int len = (int)strlen(s);
    // Volta do final da string para o começo enquanto encontrar espaços
    while (len > 0 && isspace((unsigned char)s[len - 1])) len--;
    
    // Coloca o terminador nulo logo após o último caractere válido
    s[len] = '\0';
}

/**
 * Verifica se uma string está vazia ou contém apenas espaços em branco.
 * Retorna 1 se estiver "em branco", 0 caso contrário.
 */
int str_blank(const char *s) {
    // Percorre a string caractere por caractere
    while (*s) { 
        // Se encontrar qualquer caractere que NÃO seja espaço, a string não é branca
        if (!isspace((unsigned char)*s)) return 0; 
        s++; 
    }
    return 1;
}