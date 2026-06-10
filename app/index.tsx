import { Stack } from "expo-router";
import React, { useEffect, useRef, useState } from "react";
import {
  Animated,
  SafeAreaView,
  StatusBar,
  StyleSheet,
  Text,
  TouchableOpacity,
  Vibration,
  View,
} from "react-native";
import ConfettiCannon from "react-native-confetti-cannon";

const IP_PLACAR = "192.168.4.1";
const API_URL = `http://${IP_PLACAR}`;

export default function App() {
  const [dados, setDados] = useState({
    esquerda: { pontos: 0, fogo: false, match: false },
    direita: { pontos: 0, fogo: false, match: false },
    jogoFinalizado: false,
    vencedor: 0,
  });

  const enviandoComando = useRef(false);

  // Referências para as animações nativas
  const animFogoEsq = useRef(new Animated.Value(0)).current;
  const animFogoDir = useRef(new Animated.Value(0)).current;
  const animMatchEsq = useRef(new Animated.Value(1)).current;
  const animMatchDir = useRef(new Animated.Value(1)).current;
  const animArcoIris = useRef(new Animated.Value(0)).current;

  // Referências para controlar o disparo dos canhões de confete
  const canhaoEsquerdo = useRef<any>(null);
  const canhaoDireito = useRef<any>(null);

  // Guardas para controlar se as animações em loop já estão rodando
  const loopFogoEsq = useRef<Animated.CompositeAnimation | null>(null);
  const loopFogoDir = useRef<Animated.CompositeAnimation | null>(null);
  const loopMatchEsq = useRef<Animated.CompositeAnimation | null>(null);
  const loopMatchDir = useRef<Animated.CompositeAnimation | null>(null);
  const loopArcoIris = useRef<Animated.CompositeAnimation | null>(null);

  // Efeito de Fim de Jogo: Confete e Vibração
  useEffect(() => {
    if (dados.jogoFinalizado) {
      console.log("🏆 Partida finalizada!");

      // Faz o celular vibrar por 1 segundo
      Vibration.vibrate(1000);

      // Dispara os canhões uma única vez
      canhaoEsquerdo.current?.start();
      canhaoDireito.current?.start();
    }
  }, [dados.jogoFinalizado]);

  // Carrega o estado inicial ao abrir o app
  useEffect(() => {
    const carregarEstadoInicial = async () => {
      try {
        const resposta = await fetch(`${API_URL}/status`, {
          method: "GET",
          headers: { "Cache-Control": "no-cache" },
        });
        if (resposta.ok) {
          const json = await resposta.json();
          setDados(json);
        }
      } catch (err) {
        console.log("Aguardando conexão Wi-Fi com o placar...");
      }
    };
    carregarEstadoInicial();
  }, []);

  // Envia comando e sincroniza com a resposta JSON do Arduino
  const enviarComando = async (lado: string, acao: string) => {
    if (enviandoComando.current) return;
    enviandoComando.current = true;

    try {
      const resposta = await fetch(
        `${API_URL}/controlar?lado=${lado}&acao=${acao}`,
        {
          method: "GET",
          mode: "cors",
        },
      );

      if (resposta.ok) {
        const jsonAtualizado = await resposta.json();
        setDados(jsonAtualizado);
      }
    } catch (err) {
      console.log("Erro ao processar comando via HTTP:", err);
    } finally {
      setTimeout(() => {
        enviandoComando.current = false;
      }, 200);
    }
  };

  // Gerenciamento dos Loops de Animação
  useEffect(() => {
    // Fogo Esquerda
    if (dados.esquerda.fogo) {
      if (!loopFogoEsq.current) {
        loopFogoEsq.current = Animated.loop(
          Animated.sequence([
            Animated.timing(animFogoEsq, {
              toValue: 1,
              duration: 400,
              useNativeDriver: false,
            }),
            Animated.timing(animFogoEsq, {
              toValue: 0,
              duration: 400,
              useNativeDriver: false,
            }),
          ]),
        );
        loopFogoEsq.current.start();
      }
    } else {
      if (loopFogoEsq.current) {
        loopFogoEsq.current.stop();
        loopFogoEsq.current = null;
      }
      animFogoEsq.setValue(0);
    }

    // Fogo Direita
    if (dados.direita.fogo) {
      if (!loopFogoDir.current) {
        loopFogoDir.current = Animated.loop(
          Animated.sequence([
            Animated.timing(animFogoDir, {
              toValue: 1,
              duration: 400,
              useNativeDriver: false,
            }),
            Animated.timing(animFogoDir, {
              toValue: 0,
              duration: 400,
              useNativeDriver: false,
            }),
          ]),
        );
        loopFogoDir.current.start();
      }
    } else {
      if (loopFogoDir.current) {
        loopFogoDir.current.stop();
        loopFogoDir.current = null;
      }
      animFogoDir.setValue(0);
    }

    // Match Point Esquerda
    if (dados.esquerda.match) {
      if (!loopMatchEsq.current) {
        loopMatchEsq.current = Animated.loop(
          Animated.sequence([
            Animated.timing(animMatchEsq, {
              toValue: 0.2,
              duration: 400,
              useNativeDriver: false,
            }),
            Animated.timing(animMatchEsq, {
              toValue: 1,
              duration: 400,
              useNativeDriver: false,
            }),
          ]),
        );
        loopMatchEsq.current.start();
      }
    } else {
      if (loopMatchEsq.current) {
        loopMatchEsq.current.stop();
        loopMatchEsq.current = null;
      }
      animMatchEsq.setValue(1);
    }

    // Match Point Direita
    if (dados.direita.match) {
      if (!loopMatchDir.current) {
        loopMatchDir.current = Animated.loop(
          Animated.sequence([
            Animated.timing(animMatchDir, {
              toValue: 0.2,
              duration: 400,
              useNativeDriver: false,
            }),
            Animated.timing(animMatchDir, {
              toValue: 1,
              duration: 400,
              useNativeDriver: false,
            }),
          ]),
        );
        loopMatchDir.current.start();
      }
    } else {
      if (loopMatchDir.current) {
        loopMatchDir.current.stop();
        loopMatchDir.current = null;
      }
      animMatchDir.setValue(1);
    }

    // Vitória (Arco-Íris) - Roda se jogoFinalizado for true
    if (dados.jogoFinalizado) {
      if (!loopArcoIris.current) {
        loopArcoIris.current = Animated.loop(
          Animated.timing(animArcoIris, {
            toValue: 1,
            duration: 3000,
            useNativeDriver: false,
          }),
        );
        loopArcoIris.current.start();
      }
    } else {
      if (loopArcoIris.current) {
        loopArcoIris.current.stop();
        loopArcoIris.current = null;
      }
      animArcoIris.setValue(0);
    }
  }, [dados]);

  const formatarNumero = (num: number) => (num < 10 ? `0${num}` : num);

  // Interpolações de Cores
  const corDeFundoFogoEsq = animFogoEsq.interpolate({
    inputRange: [0, 1],
    outputRange: ["rgba(255, 77, 77, 0.15)", "rgba(255, 165, 0, 0.4)"],
  });

  const corDeFundoFogoDir = animFogoDir.interpolate({
    inputRange: [0, 1],
    outputRange: ["rgba(0, 50, 150, 0.15)", "rgba(0, 255, 255, 0.4)"],
  });

  const corTextoArcoIris = animArcoIris.interpolate({
    inputRange: [0, 0.2, 0.4, 0.6, 0.8, 1],
    outputRange: [
      "#ff0000",
      "#00ff00",
      "#0000ff",
      "#ffff00",
      "#ff00ff",
      "#ff0000",
    ],
  });

  const obterEstiloTextoNumero = (lado: "esq" | "dir") => {
    if (dados.jogoFinalizado && dados.vencedor === (lado === "esq" ? 1 : 2)) {
      return { color: corTextoArcoIris };
    }
    if (lado === "esq" && dados.esquerda.fogo) return { color: "#ffaa00" };
    if (lado === "dir" && dados.direita.fogo) return { color: "#00ffff" };
    return { color: "#fff" };
  };

  const sincronizarPlacar = async () => {
    try {
      const resposta = await fetch(`${API_URL}/status`, {
        method: "GET",
        headers: { "Cache-Control": "no-cache" },
      });

      if (resposta.ok) {
        const jsonAtualizado = await resposta.json();
        setDados(jsonAtualizado); // Força o App a engolir o que está no Arduino
        console.log("Placar sincronizado com sucesso!", jsonAtualizado);
      }
    } catch (err) {
      console.log("Não foi possível sincronizar. Placar offline.");
    }
  };

  return (
    <View style={styles.containerPai}>
      <StatusBar
        barStyle='light-content'
        backgroundColor='#121212'
        translucent={true}
      />
      <SafeAreaView style={styles.safeArea}>
        <Stack.Screen options={{ headerShown: false }} />

        <TouchableOpacity
          style={styles.btnSincronizar}
          onPress={() => sincronizarPlacar()}
        >
          <Text style={styles.txtReset}>SINCRONIZAR</Text>
        </TouchableOpacity>

        <View style={styles.placarContainer}>
          {/* LADO ESQUERDO */}
          <View style={styles.colunaControle}>
            <Text style={[styles.labelTime, { color: "#ff4d4d" }]}>
              {dados.esquerda.match ? "MATCH POINT" : "ESQUERDA"}
            </Text>
            <TouchableOpacity
              style={[styles.botaoMaisMenos, { backgroundColor: "#ff4d4d" }]}
              onPress={() => enviarComando("esq", "mais")}
            >
              <Text style={styles.txtBotao}>+</Text>
            </TouchableOpacity>
            <Animated.View
              style={[
                styles.visorNumero,
                {
                  borderColor: dados.esquerda.fogo ? "#ffaa00" : "#ff4d4d",
                  opacity: animMatchEsq,
                  backgroundColor: dados.esquerda.fogo
                    ? corDeFundoFogoEsq
                    : "#1e1e1e",
                },
              ]}
            >
              <Animated.Text
                style={[styles.numeroPlacar, obterEstiloTextoNumero("esq")]}
              >
                {formatarNumero(dados.esquerda.pontos)}
              </Animated.Text>
            </Animated.View>
            <TouchableOpacity
              style={styles.btnMenos}
              onPress={() => enviarComando("esq", "menos")}
            >
              <Text style={styles.txtBotao}>-</Text>
            </TouchableOpacity>
          </View>

          {/* LADO DIREITO */}
          <View style={styles.colunaControle}>
            <Text style={[styles.labelTime, { color: "#3399ff" }]}>
              {dados.direita.match ? "MATCH POINT" : "DIREITA"}
            </Text>
            <TouchableOpacity
              style={[styles.botaoMaisMenos, { backgroundColor: "#3399ff" }]}
              onPress={() => enviarComando("dir", "mais")}
            >
              <Text style={styles.txtBotao}>+</Text>
            </TouchableOpacity>
            <Animated.View
              style={[
                styles.visorNumero,
                {
                  borderColor: dados.direita.fogo ? "#00ffff" : "#3399ff",
                  opacity: animMatchDir,
                  backgroundColor: dados.direita.fogo
                    ? corDeFundoFogoDir
                    : "#1e1e1e",
                },
              ]}
            >
              <Animated.Text
                style={[styles.numeroPlacar, obterEstiloTextoNumero("dir")]}
              >
                {formatarNumero(dados.direita.pontos)}
              </Animated.Text>
            </Animated.View>
            <TouchableOpacity
              style={styles.btnMenos}
              onPress={() => enviarComando("dir", "menos")}
            >
              <Text style={styles.txtBotao}>-</Text>
            </TouchableOpacity>
          </View>
        </View>

        <TouchableOpacity
          style={styles.btnReset}
          onPress={() => enviarComando("reset", "tudo")}
        >
          {/* O Texto "ZERAR PLACAR" agora herda a animação arco-íris se o jogo terminou */}
          <Animated.Text
            style={[
              styles.txtReset,
              dados.jogoFinalizado && {
                color: corTextoArcoIris,
                fontWeight: "bold",
                fontSize: 16,
              },
            ]}
          >
            ZERAR PLACAR
          </Animated.Text>
        </TouchableOpacity>

        {dados.jogoFinalizado && (
          <>
            <ConfettiCannon
              ref={canhaoEsquerdo}
              count={60}
              origin={{ x: -10, y: 400 }}
              autoStart={false}
              fadeOut={true}
              fallSpeed={2500}
            />
            <ConfettiCannon
              ref={canhaoDireito}
              count={60}
              origin={{ x: 400, y: 400 }}
              autoStart={false}
              fadeOut={true}
              fallSpeed={2500}
            />
          </>
        )}
      </SafeAreaView>
    </View>
  );
}

const styles = StyleSheet.create({
  containerPai: { flex: 1, backgroundColor: "#121212" },
  safeArea: {
    flex: 1,
    alignItems: "center",
    justifyContent: "center",
    paddingVertical: 50,
  },
  titulo: {
    fontSize: 20,
    fontWeight: "bold",
    color: "#fff",
    letterSpacing: 2,
    marginTop: 10,
  },
  placarContainer: {
    flexDirection: "row",
    width: "100%",
    justifyContent: "space-around",
    paddingHorizontal: 10,
  },
  colunaControle: { alignItems: "center", width: "45%" },
  labelTime: {
    fontSize: 16,
    fontWeight: "bold",
    marginBottom: 15,
    letterSpacing: 1,
  },
  visorNumero: {
    width: "100%",
    height: 140,
    borderRadius: 20,
    alignItems: "center",
    justifyContent: "center",
    marginVertical: 15,
    borderWidth: 2,
    elevation: 5,
  },
  numeroPlacar: { fontSize: 60, fontWeight: "bold" },
  botaoMaisMenos: {
    width: "80%",
    height: 55,
    borderRadius: 15,
    alignItems: "center",
    justifyContent: "center",
  },
  btnMenos: {
    backgroundColor: "#2a2a2a",
    width: "80%",
    height: 55,
    borderRadius: 15,
    alignItems: "center",
    justifyContent: "center",
  },
  txtBotao: { color: "#fff", fontSize: 28, fontWeight: "bold" },
  btnReset: {
    backgroundColor: "#1e1e1e",
    paddingHorizontal: 50,
    paddingVertical: 15,
    borderRadius: 25,
    borderWidth: 1,
    borderColor: "#444",
    position: "absolute",
    bottom: 100,
  },
  txtReset: {
    color: "#aaa",
    fontWeight: "bold",
    fontSize: 14,
    letterSpacing: 1,
  },
  btnSincronizar: {
    backgroundColor: "#1e1e1e",
    paddingHorizontal: 50,
    paddingVertical: 15,
    borderRadius: 25,
    borderWidth: 1,
    borderColor: "#444",
    position: "absolute",
    top: 100,
  },
});
