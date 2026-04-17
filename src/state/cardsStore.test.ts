import { describe, it, expect, beforeEach } from "vitest";
import { useCardsStore } from "./cardsStore";

describe("cardsStore", () => {
  beforeEach(() => {
    useCardsStore.setState({ cards: [] });
  });

  it("has correct initial state", () => {
    expect(useCardsStore.getState().cards).toEqual([]);
  });

  it("shows a card", () => {
    useCardsStore.getState().showCard({
      cardId: "card-1",
      kind: "info",
      title: "Test Card",
    });
    expect(useCardsStore.getState().cards).toHaveLength(1);
    expect(useCardsStore.getState().cards[0]?.cardId).toBe("card-1");
  });

  it("dismisses a card by id", () => {
    useCardsStore.getState().showCard({
      cardId: "card-1",
      kind: "info",
      title: "Card 1",
    });
    useCardsStore.getState().showCard({
      cardId: "card-2",
      kind: "warning",
      title: "Card 2",
    });
    useCardsStore.getState().dismissCard("card-1");
    expect(useCardsStore.getState().cards).toHaveLength(1);
    expect(useCardsStore.getState().cards[0]?.cardId).toBe("card-2");
  });

  it("clears all cards", () => {
    useCardsStore.getState().showCard({
      cardId: "card-1",
      kind: "info",
      title: "Card 1",
    });
    useCardsStore.getState().showCard({
      cardId: "card-2",
      kind: "info",
      title: "Card 2",
    });
    useCardsStore.getState().clearCards();
    expect(useCardsStore.getState().cards).toEqual([]);
  });
});
