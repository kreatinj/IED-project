import type { NextApiRequest, NextApiResponse } from 'next';

// 서버 메모리에 장애물 목록 저장
let obstacles: { x: number; y: number }[] = [];

export default function handler(req: NextApiRequest, res: NextApiResponse) {
  if (req.method === 'GET') {
    res.status(200).json(obstacles);
  } else if (req.method === 'POST') {
    const { x, y } = req.body;
    if (
      typeof x === 'number' &&
      typeof y === 'number' &&
      !obstacles.some((o) => o.x === x && o.y === y)
    ) {
      obstacles.push({ x, y });
    }
    res.status(200).json({ obstacles });
  } else if (req.method === 'DELETE') {
    const { x, y } = req.body;
    obstacles = obstacles.filter((o) => !(o.x === x && o.y === y));
    res.status(200).json({ obstacles });
  } else {
    res.setHeader('Allow', ['GET', 'POST', 'DELETE']);
    res.status(405).end(`Method ${req.method} Not Allowed`);
  }
} 