import { useEffect, useRef, useState } from 'react';
import mapImg from './assets/map.png';

const GRID_SIZE = 4; // 교차점 개수
const MAP_SIZE = 400; // px
const POLL_INTERVAL = 1000;

function getCoordPos(n) {
  const s = MAP_SIZE / 16;
  return (n - 1) * 100 + (n-1) * s + s/2;
}

const DIR_TRIANGLE = {
  N: <polygon points="15,3 27,27 3,27" fill="#f00" />, // 위
  S: <polygon points="3,3 27,3 15,27" fill="#f00" />, // 아래
  E: <polygon points="3,3 27,15 3,27" fill="#f00" />, // 오른쪽
  W: <polygon points="27,3 3,15 27,27" fill="#f00" />, // 왼쪽
};

function App() {
  const [url, setUrl] = useState('http://192.168.0.112');
  const [inputUrl, setInputUrl] = useState('http://192.168.0.112');
  const [obstacles, setObstacles] = useState([]); // {x, y}[]
  const [destination, setDestination] = useState({x: 1, y: 1}); // {x, y}
  const [location, setLocation] = useState({position: {x: 1, y: 1}, direction: 'N'}); // {x, y, direction}
  const intervalRef = useRef();

  // Polling
  useEffect(() => {
    if (!url) return;
    const poll = async () => {
      try {
        const [locRes, destRes, obsRes] = await Promise.all([
          fetch(`${url}/api/location`).then(r => r.json()),
          fetch(`${url}/api/destination`).then(r => r.json()),
          fetch(`${url}/api/obstacles`).then(r => r.json()),
        ]);
        setLocation(locRes.location);
        setDestination(destRes.destination);
        setObstacles(obsRes.obstacles || []);
      } catch {
        // 네트워크 에러 무시
      }
    };
    poll();
    intervalRef.current = setInterval(poll, POLL_INTERVAL);
    return () => clearInterval(intervalRef.current);
  }, [url]);

  // 좌표 클릭 핸들러
  const handleGridClick = async (x, y) => {
    if (!url) return;
    try {
      await fetch(`${url}/api/destination`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ destination: { x, y } }),
      });
    } catch {
      // 무시
    }
  };

  // 장애물 여부
  const isObstacle = (x, y) => obstacles.some(o => o.x === x && o.y === y);
  // 도착점 여부
  const isDestination = (x, y) => destination && destination.x === x && destination.y === y;

  // URL 입력 후 적용
  const handleUrlSubmit = e => {
    e.preventDefault();
    setUrl(inputUrl.trim());
  };

  return (
    <div className="min-h-screen w-screen flex flex-col items-center justify-center p-4">
      <div className="flex flex-col items-center justify-center w-full">
        <form onSubmit={handleUrlSubmit} className="mb-6 flex gap-2 items-center">
          <label className="text-white">URL:</label>
          <input
            type="text"
            placeholder="ex) http://192.168.0.112"
            value={inputUrl}
            onChange={e => setInputUrl(e.target.value)}
            className="border rounded px-2 py-1 w-64 bg-zinc-800 text-white"
          />
          <button type="submit" className="bg-blue-500 text-white rounded p-0">적용</button>
        </form>
        <div className="flex justify-center w-full mt-[100px]">
          <div className="relative" style={{ width: MAP_SIZE, height: MAP_SIZE }}>
            <img src={mapImg} alt="map" className="w-full h-full object-cover rounded shadow select-none pointer-events-none" />
            {/* 4x4 교차점(총 16개) */}
            {Array.from({ length: GRID_SIZE }).map((_, ix) =>
              Array.from({ length: GRID_SIZE }).map((_, iy) => {
                const x = ix + 1;
                const y = iy + 1;
                const left = getCoordPos(x);
                const top = getCoordPos(GRID_SIZE - y + 1);
                return (
                  <div
                    key={x + ',' + y}
                    style={{ left, top }}
                    className="absolute z-10 -translate-x-1/2 -translate-y-1/2 w-[40px] h-[40px]"
                  >
                    {/* 도착점: 30px 파랑 원 SVG */}
                    {isDestination(x, y) && (
                      <svg width={30} height={30} className="absolute left-1/2 top-1/2 -translate-x-1/2 -translate-y-1/2 z-20">
                        <circle cx="15" cy="15" r="12" fill="#3b82f6" />
                      </svg>
                    )}
                    {/* 장애물: 30px 초록 정사각형 SVG */}
                    {isObstacle(x, y) && (
                      <svg width={30} height={30} className="absolute left-1/2 top-1/2 -translate-x-1/2 -translate-y-1/2 z-20">
                        <rect x="3" y="3" width="24" height="24" rx="4" fill="#22c55e" />
                      </svg>
                    )}
                    {/* 클릭 가능한 버튼 */}
                    <button
                      className={`w-full h-full rounded-full border-2 border-black/80 transition-all duration-100 bg-transparent z-[100] 
                        ${isObstacle(x, y) || isDestination(x, y) ? 'opacity-0 pointer-events-none' : 'pointer-events-auto bg-white hover:bg-blue-200/60 cursor-pointer'}
                      `}
                      disabled={isObstacle(x, y) || isDestination(x, y)}
                      onClick={() => !isObstacle(x, y) && !isDestination(x, y) && handleGridClick(x, y)}
                      tabIndex={-1}
                    />
                  </div>
                );
              })
            )}
            {/* 현재 위치(삼각형)는 소수점 좌표도 허용, 지도 위에 항상 표시 */}
            {location && (
              <svg
                width="30"
                height="30"
                style={{
                  position: 'absolute',
                  left: getCoordPos(location.position.x),
                  top: getCoordPos(GRID_SIZE - location.position.y + 1),
                  transform: 'translate(-50%, -50%)',
                  zIndex: 30,
                  pointerEvents: 'none',
                  width: '30px', height: '30px' 
                }}
              >
                {DIR_TRIANGLE[location.direction]}
              </svg>
            )}
          </div>
        </div>
      </div>
    </div>
  );
}

export default App;
