"use client";
import React, { useEffect, useState } from "react";
import Image from "next/image";

const GRID_SIZE = 4;
const MAP_SIZE = 400; // px

function getCoordPos(n: number) {
  const s = MAP_SIZE / 16;
  return (n - 1) * 100 + (n - 1) * s + s / 2;
}

export default function Home() {
  const [obstacles, setObstacles] = useState<{ x: number; y: number }[]>([]);
  const [loading, setLoading] = useState(false);

  const fetchObstacles = async () => {
    const res = await fetch("/api/obstacles");
    const data = await res.json();
    setObstacles(data.obstacles);
  };

  useEffect(() => {
    fetchObstacles();
  }, []);

  const isObstacle = (x: number, y: number) =>
    obstacles.some((o) => o.x === x && o.y === y);

  const handleGridClick = async (x: number, y: number) => {
    setLoading(true);
    const method = isObstacle(x, y) ? "DELETE" : "POST";
    await fetch("/api/obstacles", {
      method,
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ x, y }),
    });
    await fetchObstacles();
    setLoading(false);
  };

  return (
    <div className="min-h-screen w-screen flex flex-col items-center justify-center p-4">
      <h1 className="text-2xl font-bold mb-6">장애물 설정 (교차점 클릭)</h1>
      <div className="relative" style={{ width: MAP_SIZE, height: MAP_SIZE }}>
        {/* 배경 지도 */}
        <Image
          src="/map.png"
          alt="map"
          className="object-cover rounded shadow select-none pointer-events-none"
          width={MAP_SIZE}
          height={MAP_SIZE}
        />
        {/* 4x4 교차점(총 16개) */}
        {Array.from({ length: GRID_SIZE }).map((_, ix) =>
          Array.from({ length: GRID_SIZE }).map((_, iy) => {
            const x = ix + 1;
            const y = iy + 1;
            const left = getCoordPos(x);
            const top = getCoordPos(GRID_SIZE - y + 1);
            const obstacle = isObstacle(x, y);
            return (
              <div
                key={x + "," + y}
                style={{ left, top }}
                className="absolute z-10 -translate-x-1/2 -translate-y-1/2 w-[40px] h-[40px]"
              >
                {/* 장애물: 30px 파랑 원 SVG (클릭 시 삭제) */}
                {obstacle && (
                  <svg
                    width={30}
                    height={30}
                    className="absolute left-1/2 top-1/2 -translate-x-1/2 -translate-y-1/2 z-20 cursor-pointer pointer-events-auto"
                    onClick={() => handleGridClick(x, y)}
                  >
                    <rect x="3" y="3" width="24" height="24" rx="4" fill="#22c55e" />
                  </svg>
                )}
                {/* 클릭 가능한 버튼 (장애물이 없을 때만 보임) */}
                <button
                  className={`w-full h-full rounded-full transition-all duration-100 bg-transparent z-[100]  ${
                    obstacle ? "opacity-0 pointer-events-none" : "hover:bg-blue-200/60 cursor-pointer pointer-events-auto"
                  }`}
                  disabled={obstacle || loading}
                  onClick={() => !obstacle && handleGridClick(x, y)}
                  tabIndex={-1}
                />
              </div>
            );
          })
        )}
      </div>
      <p className="mt-4 text-gray-600">교차점을 클릭해 장애물을 추가/삭제하세요.</p>
    </div>
  );
}
