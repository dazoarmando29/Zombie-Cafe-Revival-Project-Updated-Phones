import type { ReactElement } from 'react';
import { renderToStaticMarkup } from 'react-dom/server';
import { characters } from '../src/data/characters';
import { events } from '../src/data/events';
import { items } from '../src/data/items';
import { recipes } from '../src/data/recipes';
import { CharacterCard, CharacterModal } from '../src/components/CharacterCard';
import { EventSelector } from '../src/components/EventSelector';
import { Countdown } from '../src/components/Countdown';
import { MenuMock, menuKinds } from '../src/components/MenuMock';
import {
  OverviewView,
  PediaView,
  CollectionView,
  ShopView,
  RecipesView,
  MenusView,
  CompareView,
} from '../src/pages/Views';

export function runSmoke(): string {
  const parts: ReactElement[] = [];
  for (const ev of events) {
    parts.push(<OverviewView key={`o-${ev.id}`} ev={ev} />);
    parts.push(<PediaView key={`p-${ev.id}`} ev={ev} onOpen={() => undefined} />);
    parts.push(<ShopView key={`s-${ev.id}`} ev={ev} />);
    parts.push(<RecipesView key={`r-${ev.id}`} ev={ev} />);
    parts.push(<MenusView key={`m-${ev.id}`} ev={ev} />);
    parts.push(<CompareView key={`c-${ev.id}`} ev={ev} />);
    for (const k of menuKinds) {
      parts.push(<MenuMock key={`mm-${ev.id}-${k.id}`} kind={k.id} event={ev.id} />);
    }
    parts.push(<Countdown key={`cd-${ev.id}`} windowShort={ev.windowShort} />);
    parts.push(<EventSelector key={`es-${ev.id}`} active={ev.id} onSelect={() => undefined} />);
  }
  parts.push(<CollectionView key="col" onOpen={() => undefined} />);
  for (const c of characters) {
    parts.push(<CharacterCard key={`cc-${c.id}`} char={c} onOpen={() => undefined} />);
    parts.push(<CharacterModal key={`cm-${c.id}`} char={c} onClose={() => undefined} />);
    parts.push(<CharacterModal key={`cml-${c.id}`} char={c} onClose={() => undefined} />);
  }
  void items;
  void recipes;

  const html = parts.map((p) => renderToStaticMarkup(p)).join('\n');
  return html;
}

export function assertContent(html: string): string[] {
  const checks: [string, string][] = [
    ['Pumpkin Head card', 'Pumpkin Head'],
    ['Zombie Santa card', 'Zombie Santa'],
    ['Chocolate Bunny card', 'Chocolate Bunny'],
    ['Witch Brew recipe', 'Witch'],
    ['Yule Slime recipe', 'Yule Slime Log'],
    ['Hot Cross Brains recipe', 'Hot Cross Brains'],
    ['Haunted Table item', 'Haunted Table'],
    ['Event challenges section', 'Event Challenges'],
    ['Countdown days box', 'days'],
    ['Menu mock title Halloween', 'HAUNTED CAFE'],
    ['Menu mock title Christmas', 'UNDEAD CHRISTMAS'],
    ['Menu mock title Easter', 'ROTTEN EGG HUNT'],
    ['Menu mock title Original', 'ZOMBIE CAFE'],
    ['SVG art present', '<svg'],
  ];
  const failures: string[] = [];
  for (const [label, needle] of checks) {
    if (!html.includes(needle)) failures.push(`MISSING: ${label} (${needle})`);
  }
  const svgCount = (html.match(/<svg/g) || []).length;
  if (svgCount < 200) failures.push(`TOO FEW SVG ELEMENTS: ${svgCount}`);
  if (failures.length === 0) failures.push(`OK — all ${checks.length} checks passed, ${svgCount} svg elements rendered`);
  return failures;
}
