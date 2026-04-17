/**
 * Zustand store for HUD card state (scaffolded for M0).
 *
 * Manages pending and displayed cards (approval, proactive, info, etc.).
 * No active state management in M0 — this store exists as a typed scaffold
 * for M4 and beyond.
 *
 * @module cardsStore
 */

import { create } from "zustand";

/** Card kind enumeration. */
export type CardKind =
  | "approval"
  | "proactive"
  | "cross-user-message"
  | "guest-request"
  | "info"
  | "warning"
  | "error";

/** A single HUD card. */
export interface Card {
  /** Unique card identifier. */
  cardId: string;
  /** Card kind. */
  kind: CardKind;
  /** Card title. */
  title: string;
  /** Card body text. */
  body?: string;
}

/** Cards state shape. */
export interface CardsState {
  /** Currently displayed cards. */
  cards: Card[];
}

/** Actions available on the cards store. */
export interface CardsActions {
  /** Add a card to the display. */
  showCard: (card: Card) => void;
  /** Remove a card by id. */
  dismissCard: (cardId: string) => void;
  /** Remove all cards. */
  clearCards: () => void;
}

/** Combined store type. */
export type CardsStore = CardsState & CardsActions;

/** Scaffolded cards store — no active usage in M0. */
export const useCardsStore = create<CardsStore>((set) => ({
  cards: [],

  showCard: (card: Card) =>
    set((state) => ({
      cards: [...state.cards, card],
    })),

  dismissCard: (cardId: string) =>
    set((state) => ({
      cards: state.cards.filter((c) => c.cardId !== cardId),
    })),

  clearCards: () => set({ cards: [] }),
}));
